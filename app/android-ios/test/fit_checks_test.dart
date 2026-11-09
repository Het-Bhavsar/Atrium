import 'dart:io';

import 'package:atrium/features/pod/domain/fit_checks.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter_test/flutter_test.dart';

/// Ports of atrium_ml/fit.press_ok and position.side_from_tilt. The fit vectors are the pod's once-a-second
/// Fit Quality output for six recordings (ML session `make_test_vectors.py`); the press is their last 10 s.
void main() {
  List<bool> lastTen(String name) {
    final rows = File('test/fixtures/fit/fit_$name.csv').readAsLinesSync().skip(1).toList();
    return [for (final l in rows.sublist(rows.length - 10)) l.split(',')[8] == '1'];
  }

  test('L: a good or briefly moving press passes; dark, noisy, saturated and weak do not', () {
    expect(pressOk(lastTen('good')), isTrue);
    expect(pressOk(lastTen('moving')), isTrue);
    for (final n in ['dark', 'noisy', 'saturated', 'weak']) {
      expect(pressOk(lastTen(n)), isFalse, reason: n);
    }
    expect(pressOk([...List.filled(8, true), false, false]), isTrue);
    expect(pressOk([...List.filled(7, true), false, false, false]), isFalse);
  });

  test('K: the guided tilt names the side, or asks again when too small', () {
    final still = List<double>.filled(50, 0.02);
    List<double> tilt(double dz) => [...List.filled(25, 0.02), ...List.filled(50, 0.02 + dz)];
    expect(sideFromTilt(still, tilt(0.3)), WearSide.right);
    expect(sideFromTilt(still, tilt(-0.3)), WearSide.left);
    expect(sideFromTilt(still, tilt(0.05)), isNull);
    // The first second of the tilt is skipped: a jerk there alone does not count.
    expect(sideFromTilt(still, [...List.filled(25, 0.9), ...List.filled(50, 0.02)]), isNull);
  });
}
