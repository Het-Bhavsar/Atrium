// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/features/pod/domain/pod.dart';

/// Fit guide checks (FEATURES.md #24, #26): ports of `atrium_ml/fit.press_ok` and `atrium_ml/position.side_from_tilt`.

/// Seconds of the 10 s press that must read fit good (Fit Quality 0x0206 bit 5). [ASSUMED] until V1.
const int pressGoodS = 8;

/// After the 10 s press: the fit held good for at least [pressGoodS] of the once-a-second readings.
bool pressOk(Iterable<bool> fitGoodPerSecond) => fitGoodPerSecond.where((g) => g).length >= pressGoodS;

/// Guided side check (#26): [before] is about 2 s of raw accelerometer z (g, pod frame, 25 Hz) with the head still;
/// [during] is the 3 s after "tilt your head towards your left shoulder", whose first second is skipped. Left ear
/// down raises z on the right and lowers it on the left. Null when the tilt was too small: ask again.
WearSide? sideFromTilt(List<double> beforeZ, List<double> duringZ, {double fs = 25, double minDzG = 0.10}) {
  final d = duringZ.skip(fs.toInt()).toList();
  if (beforeZ.isEmpty || d.isEmpty) return null;
  double mean(List<double> x) => x.reduce((a, b) => a + b) / x.length;
  final dz = mean(d) - mean(beforeZ);
  if (dz.abs() < minDzG) return null;
  return dz > 0 ? WearSide.right : WearSide.left;
}
