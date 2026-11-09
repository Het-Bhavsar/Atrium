// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/pod_metrics.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:flutter_test/flutter_test.dart';

/// What the screens read in real-data builds, from the ML session's reference log (`test/fixtures/phone`):
/// 12:00 to 10:00 next day at UTC+5:30, pod on the left, asleep 23:30-07:00 with a 15 min wake at 02:00. The night
/// values must equal `nightly.summarise` in phone_night.json.
void main() {
  const dir = 'test/fixtures/phone';
  final models = SleepModels.parse(File('assets/models/sleep_v1.json').readAsStringSync());
  final log = decodeBlocks(Uint8List.fromList(File('$dir/phone_log.bin').readAsBytesSync()));
  final want = (jsonDecode(File('$dir/phone_night.json').readAsStringSync()) as Map<String, dynamic>)['night'] as Map<String, dynamic>;
  const ist = Duration(hours: 5, minutes: 30);
  // 10:00 local on the morning after, as the reference log ends.
  final now = DateTime.fromMillisecondsSinceEpoch((log.last.time + 30) * 1000);
  final m = PodMetrics(log, podOnRight: false, models: models, now: now, utcOffset: ist);
  int localMinute(num t) => ((t.round() + ist.inSeconds) % 86400) ~/ 60;

  test('two local days; last night is today', () {
    expect(m.days.length, 2);
    expect(m.lastNight, isNotNull);
    expect(m.isEmpty, isFalse);
  });

  test('last night equals the reference summary', () {
    final d = m.days.last;
    final n = m.lastNight!;
    expect(d.sleepMin, (want['asleep_min'] as num).toDouble());
    expect(d.rhr, closeTo((want['resting_hr_bpm'] as num).toDouble(), 0.01));
    expect(d.hrv, closeTo((want['rmssd_ms'] as num).toDouble(), 0.01));
    expect(n.wakeUps, want['wake_ups']);
    expect(m.bedMinute, localMinute(want['start_s'] as num));
    expect(m.wakeMinute, localMinute(want['end_s'] as num));
    expect(m.minutesOnPod, (want['on_pod_min'] as num).round());
    expect(m.hasStages, isFalse);
  });

  test('the hypnogram covers the sleep period and shows the wake-ups', () {
    final total = m.hypnogram.fold(0, (a, b) => a + b.minutes);
    expect(total, closeTo((want['period_min'] as num).toDouble(), 3));
    expect(m.hypnogram.where((b) => b.stage == SleepStage.awake).length, greaterThanOrEqualTo(want['wake_ups'] as int));
  });

  test('head positions are minutes asleep per side', () {
    final pos = want['position_min'] as Map<String, dynamic>;
    final by = {for (final p in m.headPositions) p.side: p.minutes};
    expect(by[HeadSide.back], (pos['back'] as num).round());
    expect(by[HeadSide.left], (pos['left'] as num).round());
    expect(by[HeadSide.right], (pos['right'] as num).round());
    expect(by[HeadSide.front], isNull);
  });

  test('the weekly average leaves out the day with no night logged', () {
    expect(m.series(Metric.sleepMin, Range.week).mean, (want['asleep_min'] as num).toDouble());
  });

  test('skin temperature waits for 5 nights of baseline', () {
    expect(m.days.last.tempDelta, 0);
  });

  test('steps and active minutes are summed per local day', () {
    for (final d in m.days) {
      final start = DateTime.utc(d.date.year, d.date.month, d.date.day).millisecondsSinceEpoch ~/ 1000 - ist.inSeconds;
      final day = [
        for (final e in log)
          if (e.time >= start && e.time < start + 86400) e.epoch,
      ];
      expect(d.steps, day.fold(0, (a, e) => a + e.steps));
      expect(d.active, (day.where((e) => e.activityClass == ActivityClass.walk || e.activityClass == ActivityClass.run).length * 0.5).round());
    }
  });

  test('without the model there are days but no sleep', () {
    final x = PodMetrics(log, podOnRight: false, now: now, utcOffset: ist);
    expect(x.days.length, 2);
    expect(x.days.last.sleepMin, 0);
    expect(x.lastNight, isNull);
  });

  test('an empty log is empty, not a crash', () {
    final e = PodMetrics(const [], podOnRight: false, models: models, now: now);
    expect(e.isEmpty, isTrue);
    expect(e.headPositions, isEmpty);
  });
}
