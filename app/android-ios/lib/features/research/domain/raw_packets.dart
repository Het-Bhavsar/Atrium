// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Raw signal recording (research): the pod's live PPG and motion notifications (docs/ble_gatt.md section 4.2), the
/// session file that stores them as received, and the CSV they decode to. Pure Dart.
///
/// The pod keeps no raw data on board (its log holds 30-second records only), so raw signals exist on the phone only
/// for the time it was connected and recording.
library;

import 'dart:convert';
import 'dart:typed_data';

/// Characteristics recorded (the 16-bit part of the pod's custom UUIDs).
abstract final class RawChar {
  static const int ppgData = 0x0202;
  static const int motionData = 0x0204;
}

/// One PPG FIFO item: the MAXM86161A tag (bits 23..19) and the 19-bit value (bits 18..0).
class PpgItem {
  const PpgItem(this.tag, this.value);
  final int tag;
  final int value;

  /// Tags 0x01-0x04 are the mode's slots 1-4; 0x0D-0x0F slots 1-3 after picket-fence replacement. 0x1D-0x1F (sub-DAC
  /// update, invalid, time stamp) carry no sample.
  int? get slot => switch (tag) {
    >= 0x01 && <= 0x04 => tag,
    >= 0x0D && <= 0x0F => tag - 0x0C,
    _ => null,
  };
  bool get picketFence => tag >= 0x0D && tag <= 0x0F;
  bool get isSample => slot != null;
}

/// The light a slot carries (docs/ble_gatt.md 4.2): slot 1 green, 2 IR, 3 red, 4 direct ambient (research modes 6-9).
/// Below the pod's green headroom limit, green becomes IR in the same slot (and in full the slots move up one); the
/// header doesn't say so, so the name is the slot's usual light.
String slotName(int slot) => switch (slot) {
  1 => 'green',
  2 => 'ir',
  3 => 'red',
  4 => 'ambient',
  _ => 'slot$slot',
};

/// One PPG Data notification (0x0202): u16 sequence, u8 rate (Hz), u8 mode, u32 uptime ms of the first sample (all
/// little-endian), then 3-byte FIFO items, big-endian.
class PpgPacket {
  const PpgPacket({required this.sequence, required this.rateHz, required this.mode, required this.uptimeMs, required this.items});
  final int sequence;
  final int rateHz;
  final int mode;
  final int uptimeMs;
  final List<PpgItem> items;

  static const int headerSize = 8;

  static PpgPacket? parse(Uint8List v) {
    if (v.length < headerSize) return null;
    final bd = ByteData.sublistView(v);
    return PpgPacket(
      sequence: bd.getUint16(0, Endian.little),
      rateHz: v[2],
      mode: v[3],
      uptimeMs: bd.getUint32(4, Endian.little),
      items: [for (var o = headerSize; o + 3 <= v.length; o += 3) PpgItem(v[o] >> 3, (v[o] & 0x07) << 16 | v[o + 1] << 8 | v[o + 2])],
    );
  }

  /// Each sample with its slot and its index within that slot in this packet (the k-th sample of a slot is k sample
  /// periods after [uptimeMs]).
  Iterable<({PpgItem item, int slot, int index})> samples() sync* {
    final seen = <int, int>{};
    for (final it in items) {
      final s = it.slot;
      if (s == null) continue;
      final k = seen[s] ?? 0;
      seen[s] = k + 1;
      yield (item: it, slot: s, index: k);
    }
  }
}

/// One accelerometer sample in mg, pod frame.
class MotionSample {
  const MotionSample(this.x, this.y, this.z);
  final int x;
  final int y;
  final int z;
}

/// One Motion Data notification (0x0204): u16 sequence, u16 rate, u32 uptime ms of the first sample, then 12 x
/// (i16 x, y, z in mg), little-endian. Rate: bit 15 clear, mHz; bit 15 set, bits 0-14 in 10 mHz.
class MotionPacket {
  const MotionPacket({required this.sequence, required this.rateField, required this.uptimeMs, required this.samples});
  final int sequence;
  final int rateField;
  final int uptimeMs;
  final List<MotionSample> samples;

  static const int headerSize = 8;

  double get rateHz => rateField & 0x8000 != 0 ? (rateField & 0x7FFF) / 100.0 : rateField / 1000.0;

  static MotionPacket? parse(Uint8List v) {
    if (v.length < headerSize) return null;
    final bd = ByteData.sublistView(v);
    return MotionPacket(
      sequence: bd.getUint16(0, Endian.little),
      rateField: bd.getUint16(2, Endian.little),
      uptimeMs: bd.getUint32(4, Endian.little),
      samples: [
        for (var o = headerSize; o + 6 <= v.length; o += 6)
          MotionSample(bd.getInt16(o, Endian.little), bd.getInt16(o + 2, Endian.little), bd.getInt16(o + 4, Endian.little)),
      ],
    );
  }
}

/// Missed notifications from the u16 sequence numbers (+1 per notification, wrapping at 65535). A repeat of the last
/// number is ignored; a jump back of more than half the range is taken as a restart of the pod's counter, not a gap.
class SequenceGaps {
  int? _last;

  /// Notifications missing between the ones received.
  int missed = 0;

  /// Places where at least one was missing.
  int gaps = 0;

  void add(int seq) {
    final last = _last;
    _last = seq & 0xFFFF;
    if (last == null) return;
    final step = (seq - last) & 0xFFFF;
    if (step == 0 || step == 1 || step > 0x8000) return;
    missed += step - 1;
    gaps++;
  }
}

/// What a session holds, kept while recording and rebuilt from the file when needed.
class SessionStats {
  SessionStats();

  int? startMs;
  int? endMs;
  final Map<String, int> ppgSamples = {};
  int motionSamples = 0;
  int packets = 0;
  final ppgGaps = SequenceGaps();
  final motionGaps = SequenceGaps();
  int? mode;
  int? ppgRateHz;
  double? motionRateHz;

  void add(int char, Uint8List value, int rxMs) {
    startMs ??= rxMs;
    endMs = rxMs;
    packets++;
    if (char == RawChar.ppgData) {
      final p = PpgPacket.parse(value);
      if (p == null) return;
      ppgGaps.add(p.sequence);
      mode = p.mode;
      ppgRateHz = p.rateHz;
      for (final s in p.samples()) {
        final name = slotName(s.slot);
        ppgSamples[name] = (ppgSamples[name] ?? 0) + 1;
      }
    } else if (char == RawChar.motionData) {
      final m = MotionPacket.parse(value);
      if (m == null) return;
      motionGaps.add(m.sequence);
      motionRateHz = m.rateHz;
      motionSamples += m.samples.length;
    }
  }

  Duration get duration => startMs == null ? Duration.zero : Duration(milliseconds: endMs! - startMs!);

  Map<String, Object?> toJson() => {
    'start_ms': startMs,
    'end_ms': endMs,
    'ppg_samples': ppgSamples,
    'motion_samples': motionSamples,
    'packets': packets,
    'ppg_missed': ppgGaps.missed,
    'ppg_gaps': ppgGaps.gaps,
    'motion_missed': motionGaps.missed,
    'motion_gaps': motionGaps.gaps,
    'mode': mode,
    'ppg_rate_hz': ppgRateHz,
    'motion_rate_hz': motionRateHz,
  };
}

/// The session file: 'ARW1', then one record per notification as received, little-endian:
/// u16 characteristic (0x0202 or 0x0204), i64 phone receive time (Unix ms, UTC), u16 length, the notification bytes.
abstract final class RawFile {
  static const List<int> magic = [0x41, 0x52, 0x57, 0x31]; // 'ARW1'
  static const int recordHeader = 12;

  static Uint8List record(int char, Uint8List value, int rxMs) {
    final b = ByteData(recordHeader + value.length)
      ..setUint16(0, char, Endian.little)
      ..setInt64(2, rxMs, Endian.little)
      ..setUint16(10, value.length, Endian.little);
    final out = b.buffer.asUint8List()..setRange(recordHeader, recordHeader + value.length, value);
    return out;
  }

  /// The records in [bytes]; a cut-off last record (the app stopped mid-write) is left out.
  static Iterable<({int char, int rxMs, Uint8List value})> read(Uint8List bytes) sync* {
    if (bytes.length < 4 || !_hasMagic(bytes)) return;
    final bd = ByteData.sublistView(bytes);
    var o = 4;
    while (o + recordHeader <= bytes.length) {
      final len = bd.getUint16(o + 10, Endian.little);
      if (o + recordHeader + len > bytes.length) return;
      yield (
        char: bd.getUint16(o, Endian.little),
        rxMs: bd.getInt64(o + 2, Endian.little),
        value: Uint8List.sublistView(bytes, o + recordHeader, o + recordHeader + len),
      );
      o += recordHeader + len;
    }
  }

  static bool _hasMagic(Uint8List b) => b[0] == magic[0] && b[1] == magic[1] && b[2] == magic[2] && b[3] == magic[3];

  static SessionStats stats(Uint8List bytes) {
    final s = SessionStats();
    for (final r in read(bytes)) {
      s.add(r.char, r.value, r.rxMs);
    }
    return s;
  }
}

/// PPG CSV columns (README, "Raw recordings").
const List<String> ppgCsvColumns = ['phone_rx_utc', 'phone_rx_ms', 'pod_uptime_ms', 'sequence', 'slot', 'tag', 'picket_fence', 'value', 'mode', 'rate_hz'];

/// Motion CSV columns.
const List<String> motionCsvColumns = ['phone_rx_utc', 'phone_rx_ms', 'pod_uptime_ms', 'sequence', 'x_mg', 'y_mg', 'z_mg', 'rate_hz'];

String _utc(int ms) => '${DateTime.fromMillisecondsSinceEpoch(ms, isUtc: true).toIso8601String().substring(0, 23)}Z';
String _t(double ms) => ms.toStringAsFixed(1);
String _hz(double hz) => hz == hz.roundToDouble() ? hz.toStringAsFixed(0) : hz.toStringAsFixed(2);

/// The session's PPG samples, one row each. A sample's pod time is its packet's first-sample time plus its index in
/// its slot times the sample period; the phone time is when its packet arrived.
String ppgCsv(Uint8List file) {
  final out = StringBuffer()..writeln(ppgCsvColumns.join(','));
  for (final r in RawFile.read(file)) {
    if (r.char != RawChar.ppgData) continue;
    final p = PpgPacket.parse(r.value);
    if (p == null) continue;
    final period = p.rateHz > 0 ? 1000 / p.rateHz : 0.0;
    for (final s in p.samples()) {
      out.writeln(
        [
          _utc(r.rxMs),
          r.rxMs,
          _t(p.uptimeMs + s.index * period),
          p.sequence,
          slotName(s.slot),
          '0x${s.item.tag.toRadixString(16).padLeft(2, '0')}',
          s.item.picketFence ? 1 : 0,
          s.item.value,
          p.mode,
          p.rateHz,
        ].join(','),
      );
    }
  }
  return out.toString();
}

/// The session's accelerometer samples, one row each.
String motionCsv(Uint8List file) {
  final out = StringBuffer()..writeln(motionCsvColumns.join(','));
  for (final r in RawFile.read(file)) {
    if (r.char != RawChar.motionData) continue;
    final m = MotionPacket.parse(r.value);
    if (m == null) continue;
    final period = m.rateHz > 0 ? 1000 / m.rateHz : 0.0;
    for (var i = 0; i < m.samples.length; i++) {
      final s = m.samples[i];
      out.writeln([_utc(r.rxMs), r.rxMs, _t(m.uptimeMs + i * period), m.sequence, s.x, s.y, s.z, _hz(m.rateHz)].join(','));
    }
  }
  return out.toString();
}

/// Both CSVs at once (off the UI isolate for big sessions): (ppg, motion).
(String, String) sessionCsvs(Uint8List file) => (ppgCsv(file), motionCsv(file));

/// The summary sidecar as stored next to a session file.
String statsJson(SessionStats s, {bool demo = false}) => jsonEncode({...s.toJson(), 'demo': demo});
