// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Pure-Dart entities for body metrics (FEATURES.md #1-#7). No Flutter imports.
library;

class DayMetrics {
  DayMetrics({
    required this.date,
    required this.rhr,
    required this.hrv,
    required this.sleepMin,
    required this.tempDelta,
    required this.steps,
    this.effort = 0,
    this.active = 0,
    this.recovery = 0,
    this.sleepScore = 0,
  });

  final DateTime date;
  double rhr;
  double hrv;
  double sleepMin;
  double tempDelta;
  int steps;
  int effort;
  int active;
  int recovery;
  int sleepScore;

  double value(Metric m) => switch (m) {
    Metric.rhr => rhr,
    Metric.hrv => hrv,
    Metric.sleepMin => sleepMin,
    Metric.temp => tempDelta,
    Metric.effort => effort.toDouble(),
    Metric.active => active.toDouble(),
    Metric.recovery => recovery.toDouble(),
    Metric.sleepScore => sleepScore.toDouble(),
  };
}

enum Metric { rhr, hrv, sleepMin, temp, effort, active, recovery, sleepScore }

/// The user's usual range: mean plus or minus one SD over the previous 30 days.
class Usual {
  const Usual(this.mean, this.lo, this.hi);
  final double mean;
  final double lo;
  final double hi;
}

class HrSample {
  const HrSample(this.minute, this.bpm);

  /// Minutes after midnight today (negative = yesterday).
  final int minute;
  final int bpm;
}

enum SleepStage { awake, rem, light, deep }

class StageBlock {
  const StageBlock(this.stage, this.minutes);
  final SleepStage stage;
  final int minutes;
}

/// Sleep head position (#5), while asleep only.
enum HeadSide { back, left, right, front }

class HeadPosition {
  const HeadPosition(this.side, this.minutes);
  final HeadSide side;
  final int minutes;
}

class SeriesPoint {
  const SeriesPoint(this.value, this.label);
  final double value;
  final String label;
}

class Series {
  const Series(this.points, this.xLabels);
  final List<SeriesPoint> points;

  /// (index, label) pairs for the x axis.
  final List<(int, String)> xLabels;

  /// Mean of the points with a reading: 0 marks a day without one (no night logged), so it is left out.
  double get mean {
    final xs = [
      for (final p in points)
        if (p.value != 0) p.value,
    ];
    return xs.isEmpty ? 0 : xs.reduce((a, b) => a + b) / xs.length;
  }
}

enum Range { week, month, sixMonths }

extension RangeLabel on Range {
  String get short => switch (this) {
    Range.week => 'W',
    Range.month => 'M',
    Range.sixMonths => '6M',
  };
  String get long => switch (this) {
    Range.week => '7 days',
    Range.month => '30 days',
    Range.sixMonths => '6 months',
  };
}
