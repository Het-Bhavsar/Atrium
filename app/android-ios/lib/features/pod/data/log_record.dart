// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:typed_data';

/// Pod log decoder: a port of `app/python/atrium_ml/logrec.py` (the EVT compact log).
///
/// One 12-byte record per 30 s epoch, in blocks of up to 255 records with an 8-byte header
/// (u32 time of the first epoch, u16 sequence, u8 format = 1, u8 count). Blocks never cross a 4 KB page;
/// erased flash reads 0xFF. An open block has its second header word erased and is read by counting records
/// whose byte 11 high nibble is 0.

const int epochSeconds = 30;
const int recordSize = 12;
const int headerSize = 8;
const int pageSize = 4096;
const int logFormat = 1;
const int uptimeBit = 1 << 31;

enum ActivityClass { still, walk, run, other }

class Epoch {
  const Epoch({
    required this.pulseBpm,
    required this.pulseConfidence,
    required this.onSkin,
    required this.pulseFromIr,
    required this.docked,
    required this.charging,
    required this.rmssdMs,
    required this.cleanDiffs,
    required this.activityClass,
    required this.activityMg,
    required this.mlcLevelMax,
    required this.pulsePresentS,
    required this.skinC,
    required this.steps,
    required this.pitchDeg,
    required this.rollDeg,
    required this.burstR,
    required this.burstConfidence,
  });

  /// 0 = none.
  final double pulseBpm;

  /// 0..1.
  final double pulseConfidence;
  final bool onSkin;
  final bool pulseFromIr;
  final bool docked;
  final bool charging;

  /// 0 = fewer than 5 clean differences.
  final double rmssdMs;
  final int cleanDiffs;
  final ActivityClass activityClass;
  final double activityMg;
  final int mlcLevelMax;
  final int pulsePresentS;

  /// NaN = invalid or below range.
  final double skinC;
  final int steps;

  /// Pod frame (+x up to the antenna end, +z out of the skin). NaN = unknown.
  final double pitchDeg;
  final double rollDeg;

  /// Blood-oxygen burst ratio R; 0 = no burst ended in this epoch.
  final double burstR;
  final double burstConfidence;

  static Epoch decode(Uint8List r, [int o = 0]) {
    final b1 = r[o + 1];
    final b3 = r[o + 3];
    final b5 = r[o + 5];
    final pitch = r[o + 8].toSigned(8);
    final roll = r[o + 9].toSigned(8);
    return Epoch(
      pulseBpm: r[o].toDouble(),
      pulseConfidence: (b1 & 0x0F) / 15.0,
      onSkin: b1 & 0x10 != 0,
      pulseFromIr: b1 & 0x20 != 0,
      docked: b1 & 0x40 != 0,
      charging: b1 & 0x80 != 0,
      rmssdMs: r[o + 2].toDouble(),
      cleanDiffs: b3 & 0x3F,
      activityClass: ActivityClass.values[b3 >> 6],
      activityMg: r[o + 4].toDouble(),
      mlcLevelMax: b5 & 0x07,
      pulsePresentS: b5 >> 3,
      skinC: r[o + 6] == 0 ? double.nan : 25.0 + 0.1 * r[o + 6],
      steps: r[o + 7],
      pitchDeg: pitch == -128 ? double.nan : pitch.toDouble(),
      rollDeg: roll == -128 ? double.nan : 2.0 * roll,
      burstR: r[o + 10] / 100.0,
      burstConfidence: r[o + 10] != 0 ? (r[o + 11] & 0x0F) / 15.0 : 0.0,
    );
  }
}

class TimedEpoch {
  const TimedEpoch(this.time, this.isUptime, this.epoch, [this.raw]);

  /// The 12 record bytes as logged (kept so the app can store and re-decode them).
  final Uint8List? raw;

  /// Unix seconds, or seconds since boot when [isUptime] (clock not yet set by the phone).
  final int time;
  final bool isUptime;
  final Epoch epoch;
}

class LogFormatException implements Exception {
  LogFormatException(this.message);
  final String message;
  @override
  String toString() => 'LogFormatException: $message';
}

/// Splits downloaded pages into epochs. A buffer that is not whole pages is read as one page.
List<TimedEpoch> decodeBlocks(Uint8List buf) {
  final out = <TimedEpoch>[];
  final pages = buf.length % pageSize == 0 ? [for (var i = 0; i < buf.length; i += pageSize) Uint8List.sublistView(buf, i, i + pageSize)] : [buf];
  for (final pg in pages) {
    final bd = ByteData.sublistView(pg);
    var i = 0;
    while (i + headerSize + recordSize <= pg.length) {
      final t0 = bd.getUint32(i, Endian.little);
      final w1 = bd.getUint32(i + 4, Endian.little);
      if (t0 == 0xFFFFFFFF) break;
      int n;
      if (w1 == 0xFFFFFFFF) {
        n = 0;
        while (i + headerSize + (n + 1) * recordSize <= pg.length && pg[i + headerSize + n * recordSize + 11] >> 4 == 0) {
          n++;
        }
      } else {
        final fmt = pg[i + 6];
        n = pg[i + 7];
        if (fmt != logFormat || n == 0 || i + headerSize + n * recordSize > pg.length) {
          throw LogFormatException('bad block at byte $i');
        }
      }
      i += headerSize;
      for (var k = 0; k < n; k++) {
        if (pg[i + 11] >> 4 != 0) throw LogFormatException('reserved bits set at byte $i');
        out.add(TimedEpoch((t0 & ~uptimeBit) + epochSeconds * k, t0 & uptimeBit != 0, Epoch.decode(pg, i), Uint8List.fromList(pg.sublist(i, i + recordSize))));
        i += recordSize;
      }
      if (w1 == 0xFFFFFFFF) break;
    }
  }
  return out;
}
