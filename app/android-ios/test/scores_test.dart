// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/insights/domain/metrics_snapshot.dart';
import 'package:atrium/features/insights/domain/pod_metrics.dart';
import 'package:atrium/features/insights/domain/scores.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:flutter_test/flutter_test.dart';

/// Atrium's own scores (scores_v0): Recovery against the person's usual, Sleep, Effort zones; and that the reference
/// log fills every card the screens show, with nothing written in.
void main() {
  List<double> nights(double v, [int n = 7]) => List.filled(n, v);

  group('recovery', () {
    test('no score before five nights of baseline', () {
      final r = recoveryFrom(hrv: 50, hrvHistory: nights(45, 4), rhr: 55, rhrHistory: nights(57, 4), sleepMin: 450, sleepHistory: nights(450, 4), tempDeviation: 0);
      expect(r.score, isNull);
      expect(baseline(nights(45, 4), floor: 3), isNull);
      expect(baseline(nights(45, 5), floor: 3)!.median, 45);
    });

    test('a usual night is about 50 on the heart parts; better HRV and lower resting HR raise it', () {
      RecoveryParts at(double hrv, double rhr) =>
          recoveryFrom(hrv: hrv, hrvHistory: [42, 44, 45, 46, 48, 45, 44], rhr: rhr, rhrHistory: [56, 57, 58, 57, 56, 57, 58], sleepMin: 450, sleepHistory: nights(450), tempDeviation: 0);
      final usual = at(45, 57);
      expect(usual.hrv, closeTo(50, 1));
      expect(usual.rhr, closeTo(50, 1));
      expect(at(52, 57).score! > usual.score!, isTrue);
      expect(at(45, 53).score! > usual.score!, isTrue);
      expect(at(30, 66).score! < usual.score!, isTrue);
      for (final p in [at(200, 30), at(1, 120)]) {
        expect(p.score, inInclusiveRange(0, 100));
      }
    });

    test('missing parts drop out; no HRV and no resting HR means no score', () {
      final noHrv = recoveryFrom(hrv: double.nan, hrvHistory: nights(45), rhr: 57, rhrHistory: nights(57), sleepMin: 0, sleepHistory: nights(450), tempDeviation: null);
      expect((noHrv.hrv, noHrv.sleep, noHrv.temp), (null, null, null));
      expect(noHrv.score, isNotNull);
      final none = recoveryFrom(hrv: double.nan, hrvHistory: nights(45), rhr: double.nan, rhrHistory: nights(57), sleepMin: 450, sleepHistory: nights(450), tempDeviation: 0);
      expect(none.score, isNull);
    });

    test('a warmer night than usual lowers the temperature part', () {
      double temp(double dev) =>
          recoveryFrom(hrv: 45, hrvHistory: nights(45), rhr: 57, rhrHistory: nights(57), sleepMin: 450, sleepHistory: nights(450), tempDeviation: dev).temp!;
      expect(temp(0), 100);
      expect(temp(0.4), closeTo(50, 0.01));
      expect(temp(-0.4), temp(0.4));
    });
  });

  test('sleep score: duration, efficiency and continuity', () {
    expect(sleepScore(asleepMin: 0, efficiency: 0.9, wakeUps: 0), 0);
    expect(sleepScore(asleepMin: 480, efficiency: 1, wakeUps: 0), 100);
    expect(sleepScore(asleepMin: 420, efficiency: 0.92, wakeUps: 1) < 100, isTrue);
    expect(sleepScore(asleepMin: 420, efficiency: 0.92, wakeUps: 4) < sleepScore(asleepMin: 420, efficiency: 0.92, wakeUps: 1), isTrue);
  });

  test('effort: heart-rate-reserve zones and a load that saturates', () {
    final max = maxHeartRate(34);
    expect(max, closeTo(184.2, 0.01));
    expect(maxHeartRate(null), 190);
    expect(zoneOf(70, rest: 60, max: max), -1);
    expect(zoneOf(100, rest: 60, max: max), 0);
    expect(zoneOf(170, rest: 60, max: max), 4);
    expect(effortScore([0, 0, 0, 0, 0]), 0);
    expect(effortScore([30, 0, 0, 0, 0]) < effortScore([0, 0, 30, 0, 0]), isTrue);
    expect(effortScore([600, 300, 200, 100, 60]), lessThanOrEqualTo(100));
  });

  test('the reference log fills the cards from the pod, nothing written in', () {
    final models = SleepModels.parse(File('assets/models/sleep_v1.json').readAsStringSync());
    final log = decodeBlocks(Uint8List.fromList(File('test/fixtures/phone/phone_log.bin').readAsBytesSync()));
    final now = DateTime.fromMillisecondsSinceEpoch((log.last.time + 30) * 1000);
    final m = PodMetrics(log, podOnRight: false, models: models, now: now, utcOffset: const Duration(hours: 5, minutes: 30), age: 34);
    expect(m.last.sleepScore, inInclusiveRange(1, 100));
    expect(m.lowestHr, isNotNull);
    expect(m.lowestHr!.$1, lessThanOrEqualTo(m.last.rhr.round() + 1), reason: 'the lowest 5 min is at or below the resting value');
    expect(m.restlessPerHour, isNotNull);
    expect(m.zoneMinutes, hasLength(5));
    expect(m.recoveryParts, isNull, reason: 'one night is no baseline');
    final back = MetricsSnapshot.fromJson(MetricsSnapshot.of(m).toJson())!;
    expect((back.lowestHr, back.workouts.length), (m.lowestHr, m.workouts.length));
    expect(back.zoneMinutes, m.zoneMinutes);
  });
}
