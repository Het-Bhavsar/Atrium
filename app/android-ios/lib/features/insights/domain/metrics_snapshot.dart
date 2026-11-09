// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/scores.dart';

/// A computed [MetricsSource] as plain data: what crosses back from the isolate that computes it, and what is saved
/// so the next launch draws its first frame with yesterday's numbers instead of waiting for the log to be read and
/// every night recomputed. Pure Dart.
class MetricsSnapshot extends MetricsSource {
  MetricsSnapshot({
    required this.days,
    required this.usual,
    required this.hr24,
    required this.hypnogram,
    required this.headPositions,
    required this.bedMinute,
    required this.wakeMinute,
    required this.hasStages,
    required this.today,
    this.oxygenTrend,
    this.zoneMinutes = const [],
    this.workouts = const [],
    this.lowestHr,
    this.restlessPerHour,
    this.recoveryParts,
  });

  factory MetricsSnapshot.of(MetricsSource m) => MetricsSnapshot(
    days: [...m.days],
    usual: {...m.usual},
    hr24: [...m.hr24],
    hypnogram: [...m.hypnogram],
    headPositions: [...m.headPositions],
    bedMinute: m.bedMinute,
    wakeMinute: m.wakeMinute,
    hasStages: m.hasStages,
    today: m.today,
    oxygenTrend: m.oxygenTrend,
    zoneMinutes: [...m.zoneMinutes],
    workouts: [...m.workouts],
    lowestHr: m.lowestHr,
    restlessPerHour: m.restlessPerHour,
    recoveryParts: m.recoveryParts,
  );

  /// Nothing yet (first launch).
  factory MetricsSnapshot.empty(DateTime now) => MetricsSnapshot(
    days: const [],
    usual: {for (final m in Metric.values) m: const Usual(0, 0, 0), Metric.temp: const Usual(0, -0.3, 0.3)},
    hr24: const [],
    hypnogram: const [],
    headPositions: const [],
    bedMinute: 23 * 60,
    wakeMinute: 7 * 60,
    hasStages: false,
    today: DateTime(now.year, now.month, now.day),
  );

  static const int format = 2;

  @override
  final List<DayMetrics> days;
  @override
  final Map<Metric, Usual> usual;
  @override
  final List<HrSample> hr24;
  @override
  final List<StageBlock> hypnogram;
  @override
  final List<HeadPosition> headPositions;
  @override
  final int bedMinute;
  @override
  final int wakeMinute;
  @override
  final bool hasStages;
  @override
  final DateTime today;
  @override
  final OxygenTrend? oxygenTrend;
  @override
  final List<double> zoneMinutes;
  @override
  final List<Workout> workouts;
  @override
  final (int, int)? lowestHr;
  @override
  final double? restlessPerHour;
  @override
  final RecoveryParts? recoveryParts;

  Map<String, Object?> toJson() => {
    'format': format,
    'today': _day(today),
    'days': [
      for (final d in days)
        [_day(d.date), d.rhr, d.hrv, d.sleepMin, d.tempDelta, d.steps, d.effort, d.active, d.recovery, d.sleepScore],
    ],
    'usual': {for (final e in usual.entries) e.key.name: [e.value.mean, e.value.lo, e.value.hi]},
    'hr24': [for (final h in hr24) [h.minute, h.bpm]],
    'hypnogram': [for (final b in hypnogram) [b.stage.index, b.minutes]],
    'head': [for (final h in headPositions) [h.side.index, h.minutes]],
    'bed': bedMinute,
    'wake': wakeMinute,
    'stages': hasStages,
    if (oxygenTrend != null) 'oxygen': [oxygenTrend!.change, oxygenTrend!.nights],
    'zones': zoneMinutes,
    'workouts': [for (final w in workouts) [w.kind, w.startMinute, w.minutes, w.avgBpm]],
    if (lowestHr != null) 'lowest': [lowestHr!.$1, lowestHr!.$2],
    if (restlessPerHour != null) 'restless': restlessPerHour,
    if (recoveryParts != null) 'recovery': [recoveryParts!.hrv, recoveryParts!.rhr, recoveryParts!.sleep, recoveryParts!.temp, recoveryParts!.score],
  };

  /// Null for anything that isn't a snapshot of this format (an older app, a damaged file).
  static MetricsSnapshot? fromJson(Object? j) {
    try {
      if (j is! Map<String, dynamic> || j['format'] != format) return null;
      double n(Object? v) => (v! as num).toDouble();
      int i(Object? v) => (v! as num).toInt();
      List<List<dynamic>> rows(String k) => [for (final r in j[k] as List<dynamic>) r as List<dynamic>];
      final ox = j['oxygen'] as List<dynamic>?;
      final low = j['lowest'] as List<dynamic>?;
      final rec = j['recovery'] as List<dynamic>?;
      double? on(Object? v) => v == null ? null : n(v);
      return MetricsSnapshot(
        today: DateTime.parse(j['today'] as String),
        days: [
          for (final r in rows('days'))
            DayMetrics(
              date: DateTime.parse(r[0] as String),
              rhr: n(r[1]),
              hrv: n(r[2]),
              sleepMin: n(r[3]),
              tempDelta: n(r[4]),
              steps: i(r[5]),
              effort: i(r[6]),
              active: i(r[7]),
              recovery: i(r[8]),
              sleepScore: i(r[9]),
            ),
        ],
        usual: {
          for (final e in (j['usual'] as Map<String, dynamic>).entries)
            Metric.values.byName(e.key): Usual(n((e.value as List<dynamic>)[0]), n((e.value as List<dynamic>)[1]), n((e.value as List<dynamic>)[2])),
        },
        hr24: [for (final r in rows('hr24')) HrSample(i(r[0]), i(r[1]))],
        hypnogram: [for (final r in rows('hypnogram')) StageBlock(SleepStage.values[i(r[0])], i(r[1]))],
        headPositions: [for (final r in rows('head')) HeadPosition(HeadSide.values[i(r[0])], i(r[1]))],
        bedMinute: i(j['bed']),
        wakeMinute: i(j['wake']),
        hasStages: j['stages'] == true,
        oxygenTrend: ox == null ? null : OxygenTrend(change: n(ox[0]), nights: i(ox[1])),
        zoneMinutes: [for (final v in j['zones'] as List<dynamic>) n(v)],
        workouts: [for (final r in rows('workouts')) Workout(kind: r[0] as String, startMinute: i(r[1]), minutes: i(r[2]), avgBpm: i(r[3]))],
        lowestHr: low == null ? null : (i(low[0]), i(low[1])),
        restlessPerHour: on(j['restless']),
        recoveryParts: rec == null ? null : RecoveryParts(hrv: on(rec[0]), rhr: on(rec[1]), sleep: on(rec[2]), temp: on(rec[3]), score: rec[4] == null ? null : i(rec[4])),
      );
    } on Object {
      return null;
    }
  }

  static String _day(DateTime d) => '${d.year.toString().padLeft(4, '0')}-${d.month.toString().padLeft(2, '0')}-${d.day.toString().padLeft(2, '0')}';
}
