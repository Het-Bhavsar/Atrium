// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:math' as math;

import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/scores.dart';

/// Demo data: the prototype's sample set, generated from the same seeded RNG so the
/// numbers match `design/prototype/index.html`. Used until a pod is paired.
class SampleData extends MetricsSource {
  SampleData._();

  static final SampleData instance = SampleData._().._build();

  // The prototype's written values for the cards the demo shows.
  @override
  List<double> get zoneMinutes => const [182, 64, 31, 14, 4];
  @override
  List<Workout> get workouts => const [Workout(kind: 'Run', startMinute: 430, minutes: 38, avgBpm: 134), Workout(kind: 'Walk', startMinute: 780, minutes: 30, avgBpm: 96)];
  @override
  (int, int)? get lowestHr => (49, 160);
  @override
  double? get restlessPerHour => 11;
  @override
  RecoveryParts? get recoveryParts => const RecoveryParts(hrv: 86, rhr: 80, sleep: 74, temp: 70, score: 78);

  static const int n = 182;
  static final DateTime demoToday = DateTime(2026, 11, 6);
  static const int demoBed = 23 * 60 + 8;

  @override
  DateTime get today => demoToday;
  @override
  int get bedMinute => demoBed;
  @override
  int get wakeMinute => 6 * 60 + 34;
  @override
  bool get hasStages => true;
  @override
  List<StageBlock> get hypnogram => demoHypnogram;
  @override
  List<HeadPosition> get headPositions => demoHeadPositions;

  /// Last night, bed 23:08: [stage, minutes].
  static const List<StageBlock> demoHypnogram = [
    StageBlock(SleepStage.awake, 9),
    StageBlock(SleepStage.light, 18),
    StageBlock(SleepStage.deep, 42),
    StageBlock(SleepStage.light, 24),
    StageBlock(SleepStage.rem, 14),
    StageBlock(SleepStage.awake, 3),
    StageBlock(SleepStage.light, 30),
    StageBlock(SleepStage.deep, 31),
    StageBlock(SleepStage.light, 20),
    StageBlock(SleepStage.rem, 22),
    StageBlock(SleepStage.light, 34),
    StageBlock(SleepStage.deep, 16),
    StageBlock(SleepStage.light, 21),
    StageBlock(SleepStage.rem, 28),
    StageBlock(SleepStage.awake, 4),
    StageBlock(SleepStage.light, 30),
    StageBlock(SleepStage.rem, 34),
    StageBlock(SleepStage.light, 14),
    StageBlock(SleepStage.awake, 6),
    StageBlock(SleepStage.light, 26),
    StageBlock(SleepStage.rem, 16),
    StageBlock(SleepStage.awake, 4),
  ];

  /// Sleep head position last night (#5).
  static const List<HeadPosition> demoHeadPositions = [HeadPosition(HeadSide.back, 196), HeadPosition(HeadSide.left, 131), HeadPosition(HeadSide.right, 93)];

  @override
  late final List<DayMetrics> days;
  @override
  late final Map<Metric, Usual> usual;
  @override
  late final List<HrSample> hr24;

  // mulberry32, bit-exact with the JS prototype.
  static double Function() _mulberry32(int seed) {
    var a = seed;
    int imul(int x, int y) => ((x & 0xFFFFFFFF) * (y & 0xFFFFFFFF)).toSigned(64) & 0xFFFFFFFF;
    return () {
      a = (a + 0x6D2B79F5) & 0xFFFFFFFF;
      var t = imul(a ^ (a >> 15), 1 | a);
      t = ((t + imul(t ^ (t >> 7), 61 | t)) & 0xFFFFFFFF) ^ t;
      return ((t ^ (t >> 14)) & 0xFFFFFFFF) / 4294967296.0;
    };
  }

  void _build() {
    final rnd = _mulberry32(20261106);
    double nz() => (rnd() + rnd() + rnd() - 1.5) / 0.75;
    double clamp(double v, double a, double b) => math.max(a, math.min(b, v));
    days = [];
    for (var i = 0; i < n; i++) {
      final d = demoToday.subtract(Duration(days: n - 1 - i));
      final t = i / n;
      final wk = d.weekday == DateTime.saturday || d.weekday == DateTime.sunday;
      final travel = i >= 118 && i <= 120;
      final rhr = 59 - 2.5 * t + 1.3 * nz() + (travel ? 3 : 0);
      final hrv = 43 + 3 * t + 4.5 * nz() + (i > n - 8 ? 5 : 0) - (travel ? 8 : 0);
      final sleepM = 418 + 30 * nz() + (wk ? 28 : 0) - (travel ? 70 : 0);
      final tmp = 0.13 * nz() + (travel ? 0.45 : 0);
      final steps = (8300 + 2300 * nz() + (wk ? 1800 : 0)).round();
      days.add(DayMetrics(date: d, rhr: rhr, hrv: hrv, sleepMin: sleepM, tempDelta: tmp, steps: steps));
    }
    for (var i = 0; i < n; i++) {
      final x = days[i];
      final prev = i > 0 ? days[i - 1] : x;
      x
        ..effort = clamp(x.steps / 150 + 6 * nz(), 12, 96).round()
        ..active = clamp(x.effort * 0.8 + 6 * nz(), 4, 120).round()
        ..recovery = clamp(68 + (x.hrv - 45) * 1.1 - (x.rhr - 57) * 2.2 + (x.sleepMin - 420) / 7 - (prev.effort - 55) / 8 + 4 * nz(), 22, 98).round()
        ..sleepScore = clamp(72 + (x.sleepMin - 420) / 4 + 6 * nz(), 35, 97).round();
    }
    days.last
      ..rhr = 54
      ..hrv = 52
      ..sleepMin = asleep.toDouble()
      ..tempDelta = 0.2
      ..steps = 7912
      ..effort = 58
      ..active = 49
      ..recovery = 78
      ..sleepScore = 84;

    Usual u(Metric m) {
      final xs = days.sublist(n - 1 - 30, n - 1).map((d) => d.value(m)).toList();
      final mean = xs.reduce((a, b) => a + b) / xs.length;
      final sd = math.sqrt(xs.map((v) => (v - mean) * (v - mean)).reduce((a, b) => a + b) / xs.length);
      return Usual(mean, mean - sd, mean + sd);
    }

    usual = {
      for (final m in [Metric.rhr, Metric.hrv, Metric.sleepMin, Metric.active, Metric.effort]) m: u(m),
      Metric.temp: const Usual(0, -0.3, 0.3),
    };

    final r = _mulberry32(7);
    const end = 17 * 60 + 25;
    hr24 = [];
    for (var k = 0; k < 288; k++) {
      final mAbs = end - (287 - k) * 5;
      final m = ((mAbs % 1440) + 1440) % 1440;
      final sleeping = (mAbs < 0 && m >= demoBed) || (mAbs >= 0 && m < 6 * 60 + 34);
      double v;
      if (sleeping) {
        final c = mAbs < 0 ? (m - demoBed) : (m + 1440 - demoBed);
        v = 57 - 7 * math.sin(math.pi * clamp(c / 446, 0, 1)) + 1.2 * math.sin(c / 9) + 0.8 * r();
      } else if (mAbs >= 7 * 60 + 10 && mAbs <= 7 * 60 + 48) {
        final p = (mAbs - 430) / 38;
        v = 120 + 30 * math.sin(math.pi * p) + 4 * (r() - 0.5);
      } else if (mAbs >= 13 * 60 && mAbs <= 13 * 60 + 30) {
        v = 92 + 6 * r();
      } else {
        v = 70 + 6 * math.sin(mAbs / 47) + 3 * math.sin(mAbs / 13) + 2.5 * r() + (mAbs < 0 ? -3 : 3);
      }
      hr24.add(HrSample(mAbs, v.round()));
    }
  }
}
