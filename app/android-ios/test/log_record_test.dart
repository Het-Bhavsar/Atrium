// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/pod/data/log_record.dart';
import 'package:flutter_test/flutter_test.dart';

/// The Dart decoder must match `atrium_ml/logrec.py` bit for bit. The fixture was written by
/// logrec.pack_pages (two pages, a 255-record block, gaps, and an open uptime block) and decoded in Python.
void main() {
  final buf = File('test/fixtures/log_pages.bin').readAsBytesSync();
  final expected = (jsonDecode(File('test/fixtures/log_expected.json').readAsStringSync()) as List).cast<Map<String, dynamic>>();

  test('decodes every epoch the Python decoder does', () {
    final got = decodeBlocks(Uint8List.fromList(buf));
    expect(got.length, expected.length);
    bool near(double a, Object? b) => b == null ? a.isNaN : (a - (b as num).toDouble()).abs() < 1e-9;
    for (var i = 0; i < got.length; i++) {
      final g = got[i];
      final e = expected[i];
      final ep = g.epoch;
      expect(g.time, e['t'], reason: 'time $i');
      expect(g.isUptime, e['up'], reason: 'uptime $i');
      expect(near(ep.pulseBpm, e['pulse_bpm']), isTrue, reason: 'pulse $i');
      expect(near(ep.pulseConfidence, e['pulse_conf']), isTrue, reason: 'conf $i');
      expect(ep.onSkin, e['on_skin'], reason: 'on skin $i');
      expect(ep.pulseFromIr, e['pulse_from_ir']);
      expect(ep.docked, e['docked']);
      expect(ep.charging, e['charging']);
      expect(near(ep.rmssdMs, e['rmssd_ms']), isTrue, reason: 'rmssd $i');
      expect(ep.cleanDiffs, e['n_diffs']);
      expect(ep.activityClass.name, e['activity_class']);
      expect(near(ep.activityMg, e['activity_mg']), isTrue);
      expect(ep.mlcLevelMax, e['mlc_level_max']);
      expect(ep.pulsePresentS, e['pulse_present_s']);
      expect(near(ep.skinC, e['skin_c']), isTrue, reason: 'skin $i');
      expect(ep.steps, e['steps']);
      expect(near(ep.pitchDeg, e['pitch_deg']), isTrue, reason: 'pitch $i');
      expect(near(ep.rollDeg, e['roll_deg']), isTrue, reason: 'roll $i');
      expect(near(ep.burstR, e['burst_r']), isTrue);
      expect(near(ep.burstConfidence, e['burst_conf']), isTrue);
    }
  });

  test('an erased buffer has no epochs', () {
    expect(decodeBlocks(Uint8List(pageSize)..fillRange(0, pageSize, 0xFF)), isEmpty);
  });

  test('a bad format byte is rejected', () {
    final b = Uint8List.fromList(buf.sublist(0, pageSize));
    b[6] = 9;
    expect(() => decodeBlocks(b), throwsA(isA<LogFormatException>()));
  });
}
