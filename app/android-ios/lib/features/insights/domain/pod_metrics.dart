// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:math' as math;

import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/nightly.dart';
import 'package:atrium/features/insights/domain/position.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/log_record.dart';

/// Metrics computed on the phone from the pod's synced log (real-data builds), using the ML session's methods
/// ported in `nightly.dart` and `sleep_model.dart` (ML note 2026-11-08, letters A to N):
/// - each night (18:00 to 14:00 local): rest run, sleep/wake model, sleep period, wake-ups, resting HR, pooled RMSSD,
///   skin temperature off the pod, head position and oxygen ratio (B to J, M);
/// - skin temperature as the deviation from the median of up to 14 earlier nights, after 5 nights (I);
/// - steps and active minutes per local day (N).
/// Sleep is an estimate from a model trained on wrist data and not yet checked on the head; stages stay in Labs.
/// Recovery, Sleep score and Effort have no Atrium formula yet, so they stay 0 and the screens show "learning".
class PodMetrics extends MetricsSource {
  PodMetrics(List<TimedEpoch> epochs, {required this.podOnRight, this.models, DateTime? now, Duration? utcOffset, int uprightSinceS = 0})
    : _now = now ?? DateTime.now(),
      _off = (utcOffset ?? (now ?? DateTime.now()).timeZoneOffset).inSeconds {
    _build(epochs.where((e) => !e.isUptime).toList()..sort((a, b) => a.time.compareTo(b.time)), uprightSinceS);
  }

  final bool podOnRight;

  /// The sleep model; without it there is no sleep, only days, steps and pulse.
  final SleepModels? models;
  final DateTime _now;
  final int _off;

  @override
  final List<DayMetrics> days = [];
  @override
  Map<Metric, Usual> usual = {};
  @override
  List<HrSample> hr24 = [];
  @override
  List<StageBlock> hypnogram = [];
  @override
  List<HeadPosition> headPositions = [];
  @override
  int bedMinute = 23 * 60;
  @override
  int wakeMinute = 7 * 60;
  @override
  bool get hasStages => false;
  @override
  DateTime get today => _localDay(_now.millisecondsSinceEpoch ~/ 1000);

  /// Last night's summary, or null before there is one.
  Night? lastNight;

  /// Minutes lying on the pod's own side last night (#5).
  int minutesOnPod = 0;

  /// Local calendar day of a Unix time, at the fixed UTC offset.
  DateTime _localDay(int t) {
    final u = DateTime.fromMillisecondsSinceEpoch((t + _off) * 1000, isUtc: true);
    return DateTime(u.year, u.month, u.day);
  }

  /// Unix seconds of local midnight starting [day].
  int _midnight(DateTime day) => DateTime.utc(day.year, day.month, day.day).millisecondsSinceEpoch ~/ 1000 - _off;

  void _build(List<TimedEpoch> all, int uprightSinceS) {
    if (all.isEmpty) {
      usual = {for (final m in Metric.values) m: const Usual(0, 0, 0), Metric.temp: const Usual(0, -0.3, 0.3)};
      return;
    }
    final times = [for (final e in all) e.time];
    final epochs = [for (final e in all) e.epoch];
    // Upright (J2): the running mean of walking epochs since the last pad change, kept across syncs by the store.
    final up = uprightFromLog([
      for (final e in all)
        if (e.time >= uprightSinceS) e.epoch,
    ]);
    final offH = _off / 3600;

    final skinHistory = <double>[];
    for (var day = _localDay(all.first.time); !day.isAfter(today); day = DateTime(day.year, day.month, day.day + 1)) {
      final dStart = _midnight(day);
      final dEnd = _midnight(DateTime(day.year, day.month, day.day + 1));
      // The night ending this morning: from 18:00 the day before to 14:00 today. Slice it first (the log is sorted).
      final lo = _lowerBound(times, dStart - 6 * 3600);
      final hi = _lowerBound(times, dStart + 14 * 3600);
      final dLo = _lowerBound(times, dStart);
      final dHi = _lowerBound(times, dEnd);
      if (lo == hi && dLo == dHi) continue;

      final n = models == null || lo == hi
          ? Night()
          : summarise(times.sublist(lo, hi), epochs.sublist(lo, hi), podOnRight: podOnRight, utcOffsetH: offH, nightDateS: dStart - 6 * 3600, models: models!, up: up);
      final skinDev = deviation(n.skinC, skinHistory);
      skinHistory.add(n.skinC);

      final dayEpochs = epochs.sublist(dLo, dHi);
      days.add(
        DayMetrics(
            date: day,
            rhr: n.restingHrBpm.isNaN ? 0 : n.restingHrBpm,
            hrv: n.rmssdMs.isNaN ? 0 : n.rmssdMs,
            sleepMin: n.asleepMin,
            tempDelta: skinDev ?? 0,
            steps: dayEpochs.fold(0, (a, e) => a + e.steps),
          )
          ..active = (dayEpochs.where((e) => e.activityClass == ActivityClass.walk || e.activityClass == ActivityClass.run).length * epochMin).round(),
      );
      if (day == today && n.hasSleep) _lastNight(n);
    }

    Usual u(Metric m) {
      final xs = days.sublist(math.max(0, days.length - 31), math.max(0, days.length - 1)).map((d) => d.value(m)).where((v) => v > 0).toList();
      if (xs.isEmpty) return const Usual(0, 0, 0);
      final mean = xs.reduce((a, b) => a + b) / xs.length;
      final sd = math.sqrt(xs.map((v) => (v - mean) * (v - mean)).reduce((a, b) => a + b) / xs.length);
      return Usual(mean, mean - sd, mean + sd);
    }

    usual = {for (final m in Metric.values) m: u(m), Metric.temp: const Usual(0, -0.3, 0.3)};
    _hr24(all);
  }

  static int _lowerBound(List<int> xs, int v) {
    var a = 0;
    var b = xs.length;
    while (a < b) {
      final m = (a + b) >> 1;
      if (xs[m] < v) {
        a = m + 1;
      } else {
        b = m;
      }
    }
    return a;
  }

  void _lastNight(Night n) {
    lastNight = n;
    int minuteOf(double t) {
      final s = (t.round() + _off) % 86400;
      return s ~/ 60;
    }

    bedMinute = minuteOf(n.startS);
    wakeMinute = minuteOf(n.endS);
    final blocks = <StageBlock>[];
    var cur = n.asleep.first;
    var run = 0;
    for (final a in n.asleep) {
      if (a != cur) {
        blocks.add(StageBlock(cur ? SleepStage.light : SleepStage.awake, (run * epochMin).round()));
        cur = a;
        run = 0;
      }
      run++;
    }
    blocks.add(StageBlock(cur ? SleepStage.light : SleepStage.awake, (run * epochMin).round()));
    hypnogram = blocks.where((b) => b.minutes > 0).toList();
    headPositions = [
      for (final l in [HeadLabel.back, HeadLabel.left, HeadLabel.right, HeadLabel.front])
        if ((n.positionMin[l] ?? 0) > 0) HeadPosition(toHeadSide(l)!, n.positionMin[l]!.round()),
    ];
    minutesOnPod = n.onPodMin.round();
  }

  void _hr24(List<TimedEpoch> all) {
    final end = _now.millisecondsSinceEpoch ~/ 1000;
    final start = end - 24 * 3600;
    final midnight = _midnight(today);
    var last = 0;
    hr24 = [];
    var i = _lowerBound([for (final e in all) e.time], start);
    for (var t = start; t < end; t += 300) {
      var sum = 0.0;
      var k = 0;
      while (i < all.length && all[i].time < t + 300) {
        if (all[i].time >= t && all[i].epoch.pulseBpm > 0) {
          sum += all[i].epoch.pulseBpm;
          k++;
        }
        i++;
      }
      if (k > 0) last = (sum / k).round();
      if (last > 0) hr24.add(HrSample((t - midnight) ~/ 60, last));
    }
  }
}
