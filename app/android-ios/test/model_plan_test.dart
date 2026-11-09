// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:convert';
import 'dart:io';

import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_snapshot.dart';
import 'package:atrium/features/insights/domain/model_plan.dart';
import 'package:atrium/features/insights/domain/pod_metrics.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_test/flutter_test.dart';

/// Which on-phone models run on which phone, the off-isolate compute, and the snapshot that gives the first frame
/// its numbers. Uses the ML reference log (`test/fixtures/phone`).
void main() {
  final models = SleepModels.parse(File('assets/models/sleep_v1.json').readAsStringSync());
  final log = decodeBlocks(Uint8List.fromList(File('test/fixtures/phone/phone_log.bin').readAsBytesSync()));

  group('plan', () {
    test('tiers from RAM and the low-RAM flag; unknown is mid', () {
      expect(tierFor(ramMb: 2048), DeviceTier.low);
      expect(tierFor(ramMb: 8192, lowRamDevice: true), DeviceTier.low);
      expect(tierFor(ramMb: 4096), DeviceTier.mid);
      expect(tierFor(ramMb: 8192), DeviceTier.high);
      expect(tierFor(ramMb: 0), DeviceTier.mid);
    });

    test('sleep/wake runs everywhere; the Labs stage model only with Labs on and not on low-end phones', () {
      for (final t in DeviceTier.values) {
        expect(planFor(t, labs: true).sleepWake, isTrue);
        expect(planFor(t, labs: false).sleepStages, isFalse);
      }
      expect(planFor(DeviceTier.low, labs: true).sleepStages, isFalse);
      expect(planFor(DeviceTier.mid, labs: true).sleepStages, isTrue);
      expect(planFor(DeviceTier.high, labs: true).sleepStages, isTrue);
      expect(planFor(DeviceTier.low, labs: false).liveRefresh > planFor(DeviceTier.high, labs: false).liveRefresh, isTrue);
      for (final t in DeviceTier.values) {
        expect(planFor(t, labs: false).historyDays, greaterThanOrEqualTo(183), reason: 'the six-month range');
      }
    });

    test('stages off: the stage model is not run, and sleep is asleep/awake only', () {
      final now = DateTime.fromMillisecondsSinceEpoch((log.last.time + 30) * 1000);
      final off = PodMetrics(log, podOnRight: false, models: models, now: now, utcOffset: const Duration(hours: 5, minutes: 30));
      expect(off.hasStages, isFalse);
      expect(off.lastNight!.stages, isEmpty, reason: 'skipped, not computed and hidden');
      expect(off.hypnogram.map((b) => b.stage).toSet(), {SleepStage.light, SleepStage.awake});
      final on = PodMetrics(log, podOnRight: false, models: models, now: now, utcOffset: const Duration(hours: 5, minutes: 30), stages: true);
      expect(on.hasStages, isTrue);
      expect(on.lastNight!.stages, isNotEmpty);
      expect(on.asleep, off.asleep, reason: 'stages never change the minutes asleep');
    });
  });

  group('compute and snapshot', () {
    final job = MetricsJob(log, podOnRight: false, models: models, uprightSinceS: 0, plan: planFor(DeviceTier.mid, labs: false));

    test('another isolate gives the same metrics as computing in place', () async {
      final here = computeMetrics(job);
      final there = await compute(computeMetrics, job);
      expect(jsonEncode(there.toJson()), jsonEncode(here.toJson()));
      expect(here.days, isNotEmpty);
    });

    test('a saved snapshot reads back the same, and anything else reads as nothing', () {
      final m = computeMetrics(job);
      final back = MetricsSnapshot.fromJson(jsonDecode(jsonEncode(m.toJson())))!;
      expect(jsonEncode(back.toJson()), jsonEncode(m.toJson()));
      expect(back.last.sleepMin, m.last.sleepMin);
      expect(back.asleep, m.asleep);
      expect(back.series(Metric.rhr, Range.week).points.length, m.series(Metric.rhr, Range.week).points.length);
      expect(MetricsSnapshot.fromJson({'format': 0}), isNull);
      expect(MetricsSnapshot.fromJson('junk'), isNull);
      expect(MetricsSnapshot.fromJson({'format': 1, 'days': 3}), isNull);
    });

    test('empty snapshot is the empty state', () {
      final e = MetricsSnapshot.empty(DateTime(2026, 11, 10, 9));
      expect(e.isEmpty, isTrue);
      expect(e.today, DateTime(2026, 11, 10));
    });
  });
}
