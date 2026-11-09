// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:math' as math;

import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/nightly.dart';
import 'package:atrium/features/insights/domain/position.dart';
import 'package:atrium/features/insights/domain/scores.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/log_record.dart';

/// `nightly.dart`'s function, under another name: the class has an [MetricsSource.oxygenTrend] field.
const _oxygenTrendOf = oxygenTrend;

/// Metrics computed on the phone from the pod's synced log (real-data builds), using the ML session's methods
/// ported in `nightly.dart` and `sleep_model.dart` (ML note 2026-11-08, letters A to N):
/// - each night (18:00 to 14:00 local): rest run, sleep/wake model, sleep period, wake-ups, resting HR, pooled RMSSD,
///   skin temperature off the pod, head position and oxygen ratio (B to J, M);
/// - skin temperature as the deviation from the median of up to 14 earlier nights, after 5 nights (I);
/// - steps and active minutes per local day (N).
/// Sleep is an estimate from a model trained on wrist data and not yet checked on the head; stages stay in Labs.
/// Recovery, Sleep score and Effort are Atrium's own formulas (`scores.dart`, scores_v0, [ASSUMED] constants);
/// Recovery stays empty until five nights give a baseline.
class PodMetrics extends MetricsSource {
  /// [stages] runs the Labs stage model too (the phone's model plan decides); [historyDays] bounds the days computed
  /// (the longest range any screen shows is six months).
  PodMetrics(
    List<TimedEpoch> epochs, {
    required this.podOnRight,
    this.models,
    DateTime? now,
    Duration? utcOffset,
    int uprightSinceS = 0,
    this.stages = false,
    int historyDays = 0,
    this.age,
  }) : _now = now ?? DateTime.now(),
       _off = (utcOffset ?? (now ?? DateTime.now()).timeZoneOffset).inSeconds {
    final from = historyDays > 0 ? _now.millisecondsSinceEpoch ~/ 1000 - historyDays * 86400 : 0;
    _build(epochs.where((e) => !e.isUptime && e.time >= from).toList()..sort((a, b) => a.time.compareTo(b.time)), uprightSinceS);
  }

  /// Sleep stages from the Labs model (wake / non-REM / REM) instead of asleep and awake only.
  final bool stages;

  /// From the profile: the maximum heart rate for Effort's zones.
  final int? age;

  @override
  List<double> zoneMinutes = const [];
  @override
  List<Workout> workouts = const [];
  @override
  (int, int)? lowestHr;
  @override
  double? restlessPerHour;
  @override
  RecoveryParts? recoveryParts;

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
  bool get hasStages => _hasStages;
  bool _hasStages = false;
  @override
  DateTime get today => _localDay(_now.millisecondsSinceEpoch ~/ 1000);

  /// Last night's summary, or null before there is one.
  Night? lastNight;

  /// Minutes lying on the pod's own side last night (#5).
  int minutesOnPod = 0;

  @override
  OxygenTrend? oxygenTrend;

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
    final oxygenHistory = <double>[];
    final hrvHistory = <double>[];
    final rhrHistory = <double>[];
    final sleepHistory = <double>[];
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
          : summarise(
              times.sublist(lo, hi),
              epochs.sublist(lo, hi),
              podOnRight: podOnRight,
              utcOffsetH: offH,
              nightDateS: dStart - 6 * 3600,
              models: models!,
              up: up,
              withStages: stages,
            );
      final skinDev = deviation(n.skinC, skinHistory);
      skinHistory.add(n.skinC);
      final ox = _oxygenTrendOf(n.oxygenRatio, oxygenHistory);
      final oxNights = oxygenHistory.sublist(math.max(0, oxygenHistory.length - 14)).where((x) => !x.isNaN).length;
      oxygenHistory.add(n.oxygenRatio);

      final rp = _recovery(n, skinDev, hrvHistory, rhrHistory, sleepHistory);
      final rest = baseline(rhrHistory, floor: 2)?.median ?? (n.restingHrBpm.isNaN ? 60.0 : n.restingHrBpm);
      if (!n.rmssdMs.isNaN) hrvHistory.add(n.rmssdMs);
      if (!n.restingHrBpm.isNaN) rhrHistory.add(n.restingHrBpm);
      if (n.asleepMin > 0) sleepHistory.add(n.asleepMin);

      final dayEpochs = epochs.sublist(dLo, dHi);
      final zones = _zoneMinutes(dayEpochs, rest: rest);
      days.add(
        DayMetrics(
          date: day,
          rhr: n.restingHrBpm.isNaN ? 0 : n.restingHrBpm,
          hrv: n.rmssdMs.isNaN ? 0 : n.rmssdMs,
          sleepMin: n.asleepMin,
          tempDelta: skinDev ?? 0,
          steps: dayEpochs.fold(0, (a, e) => a + e.steps),
          recovery: rp.score ?? 0,
          sleepScore: sleepScore(asleepMin: n.asleepMin, efficiency: n.efficiency, wakeUps: n.wakeUps),
          effort: dayEpochs.isEmpty ? 0 : effortScore(zones),
        )..active = (dayEpochs.where((e) => e.activityClass == ActivityClass.walk || e.activityClass == ActivityClass.run).length * epochMin).round(),
      );
      if (day == today) {
        zoneMinutes = zones;
        workouts = _workouts(times.sublist(dLo, dHi), dayEpochs, dStart);
        recoveryParts = rp.score == null ? null : rp;
      }
      if (day == today && n.hasSleep) {
        _lastNight(n);
        _nightDetail(n, times, epochs, dStart);
        oxygenTrend = ox == null ? null : OxygenTrend(change: ox, nights: oxNights);
      }
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

  static RecoveryParts _recovery(Night n, double? skinDev, List<double> hrv, List<double> rhr, List<double> sleep) => recoveryFrom(
    hrv: n.rmssdMs,
    hrvHistory: hrv,
    rhr: n.restingHrBpm,
    rhrHistory: rhr,
    sleepMin: n.asleepMin,
    sleepHistory: sleep,
    tempDeviation: skinDev,
  );

  /// Minutes in each Effort zone, from epochs with a confident pulse on skin.
  List<double> _zoneMinutes(List<Epoch> day, {required double rest}) {
    final max = maxHeartRate(age);
    final out = List<double>.filled(effortZones.length, 0);
    for (final e in day) {
      if (!e.onSkin || e.docked || e.pulseConfidence < 0.5) continue;
      final z = zoneOf(e.pulseBpm, rest: rest, max: max);
      if (z >= 0) out[z] += epochMin;
    }
    return out;
  }

  /// Walks and runs of at least 10 min (gaps up to 2 min joined), from the pod's activity class.
  List<Workout> _workouts(List<int> t, List<Epoch> day, int midnight) {
    bool moving(Epoch e) => e.activityClass == ActivityClass.walk || e.activityClass == ActivityClass.run;
    final out = <Workout>[];
    var i = 0;
    while (i < day.length) {
      if (!moving(day[i])) {
        i++;
        continue;
      }
      var j = i;
      var last = i;
      while (j < day.length && (moving(day[j]) || t[j] - t[last] <= 120)) {
        if (moving(day[j])) last = j;
        j++;
      }
      final span = day.sublist(i, last + 1);
      final minutes = ((t[last] - t[i]) / 60 + epochMin).round();
      if (minutes >= 10) {
        final runs = span.where((e) => e.activityClass == ActivityClass.run).length;
        final pulses = [
          for (final e in span)
            if (e.pulseBpm > 0 && e.pulseConfidence >= 0.5) e.pulseBpm,
        ];
        out.add(
          Workout(
            kind: runs * 2 > span.where(moving).length ? 'Run' : 'Walk',
            startMinute: (t[i] - midnight) ~/ 60,
            minutes: minutes,
            avgBpm: pulses.isEmpty ? 0 : (pulses.reduce((a, b) => a + b) / pulses.length).round(),
          ),
        );
      }
      i = last + 1;
    }
    return out;
  }

  /// Last night's lowest 5-min pulse and its time, and moves per hour asleep (motion above the rest threshold).
  void _nightDetail(Night n, List<int> times, List<Epoch> epochs, int midnight) {
    final lo = _lowerBound(times, n.startS.round());
    final hi = _lowerBound(times, n.endS.round());
    if (hi <= lo) return;
    const w = 10; // 5 min of 30 s epochs
    double? best;
    var at = 0;
    for (var i = lo; i + w <= hi; i++) {
      final xs = [
        for (var k = i; k < i + w; k++)
          if (epochs[k].pulseBpm > 0 && epochs[k].pulseConfidence >= 0.5) epochs[k].pulseBpm,
      ];
      if (xs.length < w * 0.7) continue;
      final m = xs.reduce((a, b) => a + b) / xs.length;
      if (best == null || m < best) {
        best = m;
        at = times[i + w ~/ 2];
      }
    }
    if (best != null) lowestHr = (best.round(), (at - midnight) ~/ 60);
    final moves = [for (var i = lo; i < hi; i++) epochs[i]].where((e) => e.onSkin && e.activityMg > const NightConfig().restMg).length;
    final hours = (hi - lo) * epochMin / 60;
    if (hours > 0) restlessPerHour = moves / hours;
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
    if (stages && n.stages.length == n.asleep.length && n.stages.isNotEmpty) {
      // Sleep/wake decides asleep or awake, so the minutes asleep never depend on Labs; the stage model only splits the
      // asleep epochs into REM and non-REM (shown as light: it doesn't tell light from deep).
      hypnogram = _blocks([
        for (var i = 0; i < n.asleep.length; i++)
          !n.asleep[i]
              ? SleepStage.awake
              : n.stages[i] == 2
              ? SleepStage.rem
              : SleepStage.light,
      ]);
      _hasStages = true;
    } else {
      hypnogram = _blocks([for (final a in n.asleep) a ? SleepStage.light : SleepStage.awake]);
    }
    headPositions = [
      for (final l in [HeadLabel.back, HeadLabel.left, HeadLabel.right, HeadLabel.front])
        if ((n.positionMin[l] ?? 0) > 0) HeadPosition(toHeadSide(l)!, n.positionMin[l]!.round()),
    ];
    minutesOnPod = n.onPodMin.round();
  }

  /// Runs of one stage as blocks. Minutes are rounded on the running total, so 30 s epochs never add up to more (or
  /// less) than the night, however many blocks there are.
  List<StageBlock> _blocks(List<SleepStage> per) {
    final blocks = <StageBlock>[];
    var start = 0;
    for (var i = 1; i <= per.length; i++) {
      if (i == per.length || per[i] != per[start]) {
        final m = (i * epochMin).round() - (start * epochMin).round();
        if (m > 0) blocks.add(StageBlock(per[start], m));
        start = i;
      }
    }
    return blocks;
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
