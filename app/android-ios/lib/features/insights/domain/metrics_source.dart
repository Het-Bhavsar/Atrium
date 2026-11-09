// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:math' as math;

import 'package:atrium/features/insights/domain/metrics.dart';

/// What every screen reads. Two implementations:
/// - `SampleData`: the prototype's demo set (default build);
/// - `PodMetrics`: computed from the pod's synced log (`--dart-define=ATRIUM_REAL_DATA=true`).
abstract class MetricsSource {
  /// One entry per day, oldest first.
  List<DayMetrics> get days;

  /// True when no readings exist yet (real mode before the first night).
  bool get isEmpty => days.isEmpty;

  /// The latest day; an all-zero day for today before the first night (screens show their empty state).
  DayMetrics get last => days.isEmpty ? DayMetrics(date: today, rhr: 0, hrv: 0, sleepMin: 0, tempDelta: 0, steps: 0) : days.last;

  /// Usual ranges (mean +- 1 SD over the previous 30 days).
  Map<Metric, Usual> get usual;

  /// The last 24 h of pulse, 5-min samples, minutes relative to midnight today.
  List<HrSample> get hr24;

  /// Last night.
  List<StageBlock> get hypnogram;
  List<HeadPosition> get headPositions;

  /// Bed time, minutes after midnight (the night before), and wake time (this morning).
  int get bedMinute;
  int get wakeMinute;

  /// True when sleep stages come from the trained model; false when only asleep/awake is known.
  bool get hasStages;

  /// Labs (#8): last night's overnight red/IR ratio against the user's own earlier nights; null before 7 of them
  /// (and always in the demo). A relative research trend, never a blood-oxygen percentage.
  OxygenTrend? get oxygenTrend => null;
  DateTime get today;

  int stageMinutes(SleepStage s) => hypnogram.where((b) => b.stage == s).fold(0, (a, b) => a + b.minutes);
  int get inBed => hypnogram.fold(0, (a, b) => a + b.minutes);
  int get asleep => inBed - stageMinutes(SleepStage.awake);

  /// Week and month: one point per day. Six months: weekly means.
  Series series(Metric metric, Range range) {
    final n = days.length;
    if (n == 0) return const Series([], []);
    if (range != Range.sixMonths) {
      final k = math.min(range == Range.week ? 7 : 30, n);
      final sl = days.sublist(n - k);
      final pts = [for (final x in sl) SeriesPoint(x.value(metric), '${dow(x.date)} ${dayLabel(x.date)}')];
      final xl = range == Range.week
          ? [for (var i = 0; i < sl.length; i++) (i, dow(sl[i].date).substring(0, 1))]
          : [
              for (final i in [0, 7, 14, 21, 29])
                if (i < sl.length) (i, dayLabel(sl[i].date)),
            ];
      return Series(pts, xl);
    }
    final weeks = <List<DayMetrics>>[];
    for (var i = n % 7; i < n; i += 7) {
      weeks.add(days.sublist(i, math.min(i + 7, n)));
    }
    if (weeks.isEmpty) weeks.add(days);
    final pts = [for (final w in weeks) SeriesPoint(w.map((x) => x.value(metric)).reduce((a, b) => a + b) / w.length, 'Week of ${dayLabel(w.first.date)}')];
    final xl = <(int, String)>[
      for (var i = 0; i < weeks.length; i++)
        if (weeks[i].first.date.day <= 7) (i, _mon[weeks[i].first.date.month - 1]),
    ];
    return Series(pts, xl);
  }
}

/// Last night's oxygen-trend ratio relative to the median of up to 14 earlier nights (`nightly.dart` `oxygenTrend`).
class OxygenTrend {
  const OxygenTrend({required this.change, required this.nights});

  /// (tonight - baseline) / baseline of the burst ratio R.
  final double change;

  /// Earlier nights with bursts that form the baseline (7 to 14).
  final int nights;

  /// Tonight's ratio as a multiple of the baseline, e.g. 1.03.
  double get factor => 1 + change;
}

const _mon = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
const _dow = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
String dayLabel(DateTime d) => '${d.day} ${_mon[d.month - 1]}';
String dow(DateTime d) => _dow[d.weekday - 1];

String clockLabel(int minute) {
  final m = ((minute % 1440) + 1440) % 1440;
  return '${(m ~/ 60).toString().padLeft(2, '0')}:${(m % 60).toString().padLeft(2, '0')}';
}
