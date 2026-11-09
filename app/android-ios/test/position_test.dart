import 'dart:convert';
import 'dart:io';

import 'package:atrium/features/insights/domain/position.dart';
import 'package:flutter_test/flutter_test.dart';

/// The head-position classifier (#5) must label exactly as `atrium_ml/position.py` does. The fixture holds
/// random logged pitch/roll pairs for three calibrated verticals and both wear sides, classified in Python.
void main() {
  final cases = (jsonDecode(File('test/fixtures/position_expected.json').readAsStringSync()) as List).cast<Map<String, dynamic>>();

  test('labels each epoch the way the Python classifier does', () {
    var n = 0;
    for (final c in cases) {
      final up = (c['up'] as List).cast<num>();
      final frame = headFrame(Vec3(up[0].toDouble(), up[1].toDouble(), up[2].toDouble()), podOnRight: c['side'] == 'right');
      for (final v in (c['vectors'] as List).cast<List<dynamic>>()) {
        final got = classifyVector(gravityFromPitchRoll((v[0] as num).toDouble(), (v[1] as num).toDouble()), frame);
        expect(got.name, v[2], reason: 'pitch ${v[0]} roll ${v[1]} side ${c['side']}');
        n++;
      }
      final seq = [for (final p in (c['seq'] as List).cast<List<dynamic>>()) gravityFromPitchRoll((p[0] as num).toDouble(), (p[1] as num).toDouble())];
      expect(classifyEpochs(seq, frame).map((l) => l.name).toList(), c['seq_labels']);
    }
    expect(n, 720);
  });

  test('a new position must hold two epochs before it counts', () {
    final frame = headFrame(const Vec3(1, 0, 0), podOnRight: true);
    final back = gravityFromPitchRoll(0, -90); // world up along the head's forward axis: face up
    final upright = gravityFromPitchRoll(-90, 0); // +x (the long axis) straight up
    expect(classifyVector(upright, frame), HeadLabel.upright);
    expect(classifyVector(back, frame), HeadLabel.back);
    expect(classifyVector(gravityFromPitchRoll(0, 90), frame), HeadLabel.front);
    final labels = classifyEpochs([upright, upright, back, upright, back, back, back], frame);
    expect(labels.sublist(0, 5), everyElement(HeadLabel.upright));
    expect(labels.sublist(5), everyElement(HeadLabel.back));
  });

  test('walking calibration falls back to the pod long axis with too few epochs', () {
    final v = uprightFromWalking(List.filled(5, const Vec3(0, 0, 1)));
    expect([v.x, v.y, v.z], [1, 0, 0]);
    final w = uprightFromWalking(List.filled(25, const Vec3(0, 0, 2)));
    expect(w.z, closeTo(1, 1e-9));
  });
}
