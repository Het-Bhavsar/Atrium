// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:math' as math;
import 'dart:typed_data';

import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:flutter_test/flutter_test.dart';

/// PPG Data 0x0202 (ble_gatt.md section 4.2), which the fit guide draws in real builds instead of a made-up curve.
void main() {
  Uint8List chunk(List<(int tag, int value)> items, {int seq = 7, int rate = 25, int mode = 1}) {
    final b = BytesBuilder()..add([seq & 0xFF, seq >> 8, rate, mode, 0, 0, 0, 0]);
    for (final (tag, v) in items) {
      final w = tag << 19 | (v & 0x7FFFF);
      b.add([w >> 16 & 0xFF, w >> 8 & 0xFF, w & 0xFF]);
    }
    return b.toBytes();
  }

  test('keeps the green slot (tag 1, and 0x0D after picket-fence replacement), big-endian, 19-bit values', () {
    final c = PpgChunk.parse(chunk([(0x01, 0x7FFFF), (0x02, 1234), (0x0D, 300000), (0x1E, 5), (0x01, 42)]))!;
    expect((c.sequence, c.rateHz, c.mode), (7, 25, 1));
    expect(c.green, [0x7FFFF, 300000, 42]);
    expect(PpgChunk.parse(Uint8List(5)), isNull);
    expect(PpgChunk.parse(chunk([]))!.green, isEmpty);
  });

  test('the drawn trace is the signal itself, flipped and scaled; nothing is drawn without samples', () {
    expect(ppgTrace(const []), isEmpty);
    expect(ppgTrace(const [1, 2, 3]), isEmpty, reason: 'too few samples to draw');
    // A pulse is a dip in the light: the smallest raw value becomes the top of the trace.
    final raw = [for (var i = 0; i < 100; i++) (200000 - 3000 * math.sin(2 * math.pi * i / 25)).round()];
    final tr = ppgTrace(raw);
    expect(tr.length, raw.length);
    expect(tr.reduce(math.max), closeTo(1, 1e-9));
    final lowest = raw.indexOf(raw.reduce(math.min));
    expect(tr[lowest], closeTo(1, 0.01));
    expect(ppgTrace(List.filled(20, 5)).every((x) => x == 0), isTrue, reason: 'a flat signal stays flat');
  });

  test('the fit guide also turns on the raw stream (PPG Control bits 0 and 2)', () {
    expect(PodLink.fitControl & 0x04, 0x04);
    expect(PodLink.fitControl & 0x01, 0x01);
  });
}
