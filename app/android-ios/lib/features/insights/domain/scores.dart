// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Atrium's own scores (FEATURES #6 Effort, #7 Recovery, and the Sleep score), computed on the phone from what the pod
/// logs every 30 s. The formulas are Atrium's, documented here and versioned by [scoresVersion]; every constant is
/// [ASSUMED] until EVT and V1-V4 nights exist to tune it. Wellness readings, not a diagnosis. Pure Dart.
///
/// Recovery (0-100), from last night against the person's own usual (median of up to 14 earlier nights, after 5):
///   HRV          50 + 25 z   (higher than usual is better)          weight 0.35
///   resting HR   50 - 25 z   (lower than usual is better)           weight 0.25
///   sleep        100 x asleep / usual asleep (6.5-9 h), capped      weight 0.25
///   skin temp    100 - 125 |deviation in C|                         weight 0.15
///   z = (tonight - median) / spread, spread = max(1.4826 MAD, a floor: 3 ms HRV, 2 bpm resting HR).
///   Parts without a reading drop out and the weights renormalise; no HRV and no resting HR means no score.
/// Sleep score (0-100): 0.5 duration (asleep / 8 h, capped) + 0.3 efficiency + 0.2 continuity (100 - 15 per wake-up).
/// Effort (0-100): minutes in five heart-rate-reserve zones (Karvonen; maximum 208 - 0.7 age, Tanaka, or 190 without
///   an age; rest from the usual resting HR, or 60), weighted 1-5, load L; Effort = 100 (1 - e^(-L / 150)).
library;

import 'dart:math' as math;


const String scoresVersion = 'scores_v0';

/// Nights before a baseline exists (ML note, I).
const int baselineNights = 5;

/// Median and robust spread of up to the last 14 values (NaN and 0 skipped); null before [baselineNights].
({double median, double spread})? baseline(List<double> history, {required double floor}) {
  final xs = history.sublist(math.max(0, history.length - 14)).where((v) => !v.isNaN && v > 0).toList()..sort();
  if (xs.length < baselineNights) return null;
  final med = _median(xs);
  final mad = _median([for (final x in xs) (x - med).abs()]..sort());
  return (median: med, spread: math.max(1.4826 * mad, floor));
}

double _median(List<double> sorted) {
  final n = sorted.length;
  return n.isOdd ? sorted[n ~/ 2] : (sorted[n ~/ 2 - 1] + sorted[n ~/ 2]) / 2;
}

double _clamp(double v) => v.clamp(0, 100).toDouble();

/// What Recovery was made of: each part 0-100, null when there was no reading.
class RecoveryParts {
  const RecoveryParts({this.hrv, this.rhr, this.sleep, this.temp, this.score});
  final double? hrv;
  final double? rhr;
  final double? sleep;
  final double? temp;

  /// The weighted score, or null (no baseline yet, or neither HRV nor resting HR last night).
  final int? score;
}

RecoveryParts recoveryFrom({
  required double hrv,
  required List<double> hrvHistory,
  required double rhr,
  required List<double> rhrHistory,
  required double sleepMin,
  required List<double> sleepHistory,
  required double? tempDeviation,
}) {
  final bh = baseline(hrvHistory, floor: 3);
  final br = baseline(rhrHistory, floor: 2);
  final bs = baseline(sleepHistory, floor: 1);
  final pHrv = bh == null || hrv.isNaN || hrv <= 0 ? null : _clamp(50 + 25 * (hrv - bh.median) / bh.spread);
  final pRhr = br == null || rhr.isNaN || rhr <= 0 ? null : _clamp(50 - 25 * (rhr - br.median) / br.spread);
  final usualSleep = (bs?.median ?? 450).clamp(390, 540).toDouble();
  final pSleep = sleepMin <= 0 ? null : _clamp(100 * sleepMin / usualSleep);
  final pTemp = tempDeviation == null || tempDeviation.isNaN ? null : _clamp(100 - 125 * tempDeviation.abs());
  int? score;
  if (pHrv != null || pRhr != null) {
    final parts = [(pHrv, 0.35), (pRhr, 0.25), (pSleep, 0.25), (pTemp, 0.15)].where((p) => p.$1 != null);
    final w = parts.fold(0.0, (a, p) => a + p.$2);
    score = (parts.fold(0.0, (a, p) => a + p.$1! * p.$2) / w).round();
  }
  return RecoveryParts(hrv: pHrv, rhr: pRhr, sleep: pSleep, temp: pTemp, score: score);
}

/// 0 when there was no sleep.
int sleepScore({required double asleepMin, required double efficiency, required int wakeUps}) {
  if (asleepMin <= 0) return 0;
  final duration = _clamp(100 * asleepMin / 480);
  final eff = _clamp(100 * (efficiency.isNaN ? asleepMin / math.max(asleepMin, 1) : efficiency));
  final cont = _clamp(100 - 15.0 * wakeUps);
  return (0.5 * duration + 0.3 * eff + 0.2 * cont).round();
}

/// Zone names, lowest first, with the heart-rate-reserve fraction each starts at.
const List<(String, double)> effortZones = [('Easy', 0.3), ('Steady', 0.5), ('Brisk', 0.6), ('Hard', 0.7), ('Peak', 0.85)];

double maxHeartRate(int? age) => age == null ? 190 : 208 - 0.7 * age;

/// Zone index (0-4) for one pulse, or -1 below the first zone.
int zoneOf(double bpm, {required double rest, required double max}) {
  if (bpm <= 0 || max <= rest) return -1;
  final hrr = (bpm - rest) / (max - rest);
  for (var z = effortZones.length - 1; z >= 0; z--) {
    if (hrr >= effortZones[z].$2) return z;
  }
  return -1;
}

/// 0-100 from minutes per zone.
int effortScore(List<double> zoneMinutes) {
  var load = 0.0;
  for (var z = 0; z < zoneMinutes.length; z++) {
    load += zoneMinutes[z] * (z + 1);
  }
  return (100 * (1 - math.exp(-load / 150))).round();
}

/// One stretch of walking or running, found from the pod's activity class.
class Workout {
  const Workout({required this.kind, required this.startMinute, required this.minutes, required this.avgBpm});
  final String kind;

  /// Minutes after local midnight.
  final int startMinute;
  final int minutes;
  final int avgBpm;
}
