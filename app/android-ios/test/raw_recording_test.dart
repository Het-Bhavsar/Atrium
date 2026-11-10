// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/storage/storage_usage.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/research/data/raw_recorder.dart';
import 'package:atrium/features/research/data/raw_store.dart';
import 'package:atrium/features/research/domain/raw_packets.dart';
import 'package:atrium/features/research/presentation/recordings_view.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

/// Raw signal recording: the pod's PPG and motion notifications (docs/ble_gatt.md 4.2), sequence gaps, the session
/// file and its CSVs, the size limit, PPG Control shared with the fit guide, and the storage breakdown.

/// A PPG Data notification: header, then each (tag, value) as a big-endian 3-byte FIFO word.
Uint8List ppg(int seq, List<(int, int)> items, {int rate = 25, int mode = 1, int uptime = 1000}) {
  final b = ByteData(8 + items.length * 3)
    ..setUint16(0, seq, Endian.little)
    ..setUint8(2, rate)
    ..setUint8(3, mode)
    ..setUint32(4, uptime, Endian.little);
  for (var i = 0; i < items.length; i++) {
    final w = items[i].$1 << 19 | items[i].$2;
    b
      ..setUint8(8 + i * 3, w >> 16)
      ..setUint8(9 + i * 3, w >> 8 & 0xFF)
      ..setUint8(10 + i * 3, w & 0xFF);
  }
  return b.buffer.asUint8List();
}

/// A Motion Data notification with [samples] (x, y, z) in mg.
Uint8List motion(int seq, List<(int, int, int)> samples, {int rate = 12500, int uptime = 1000}) {
  final b = ByteData(8 + samples.length * 6)
    ..setUint16(0, seq, Endian.little)
    ..setUint16(2, rate, Endian.little)
    ..setUint32(4, uptime, Endian.little);
  for (var i = 0; i < samples.length; i++) {
    b
      ..setInt16(8 + i * 6, samples[i].$1, Endian.little)
      ..setInt16(10 + i * 6, samples[i].$2, Endian.little)
      ..setInt16(12 + i * 6, samples[i].$3, Endian.little);
  }
  return b.buffer.asUint8List();
}

class _Host extends PodHostApi {
  final writes = <int>[];
  final notify = <(int, bool)>[];
  @override
  Future<void> write(int characteristic, Uint8List value, bool withResponse) async {
    if (characteristic == Gatt.ppgControl) writes.add(value.single);
  }

  @override
  Future<void> setNotify(int characteristic, bool enabled) async => notify.add((characteristic, enabled));
}

class _Source implements RawSource {
  final _up = StreamController<bool>.broadcast();
  final _packets = StreamController<(int, Uint8List)>.broadcast();
  bool up = false;
  int starts = 0;
  int stops = 0;

  @override
  bool get demo => false;
  @override
  bool get isConnected => up;
  @override
  Stream<bool> get connected => _up.stream;
  @override
  Stream<(int, Uint8List)> get packets => _packets.stream;
  @override
  Future<void> start() async => starts++;
  @override
  Future<void> stop() async => stops++;

  void connect(bool on) {
    up = on;
    _up.add(on);
  }

  void send(int char, Uint8List v) => _packets.add((char, v));
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  late Directory tmp;
  setUp(() => tmp = Directory.systemTemp.createTempSync('atrium_raw'));
  tearDown(() => tmp.deleteSync(recursive: true));

  group('PPG Data', () {
    test('every tag: slots 1-4, picket-fence 0x0D-0x0F, and the non-sample tags skipped', () {
      final p = PpgPacket.parse(
        ppg(
          513,
          [(0x01, 0x7FFFF), (0x02, 1), (0x03, 2), (0x04, 3), (0x0D, 4), (0x0E, 5), (0x0F, 6), (0x1D, 7), (0x1E, 8), (0x1F, 9), (0x01, 10)],
          rate: 100,
          mode: 6,
          uptime: 123456,
        ),
      )!;
      expect((p.sequence, p.rateHz, p.mode, p.uptimeMs), (513, 100, 6, 123456));
      expect(p.items, hasLength(11));
      expect(p.items.first.value, 0x7FFFF, reason: 'all 19 bits');
      final s = p.samples().toList();
      expect([for (final x in s) slotName(x.slot)], ['green', 'ir', 'red', 'ambient', 'green', 'ir', 'red', 'green']);
      expect([for (final x in s) x.item.picketFence], [false, false, false, false, true, true, true, false]);
      expect([for (final x in s) x.index], [0, 0, 0, 0, 1, 1, 1, 2], reason: 'the k-th sample of its slot');
      expect(PpgPacket.parse(Uint8List(7)), isNull);
      expect(PpgPacket.parse(ppg(0, const []))!.items, isEmpty);
    });

    test('a word split across a cut-off packet end is not read', () {
      final v = ppg(1, [(0x01, 5), (0x01, 6)]);
      expect(PpgPacket.parse(Uint8List.sublistView(v, 0, v.length - 1))!.items, hasLength(1));
    });
  });

  group('Motion Data', () {
    test('sequence, rate in mHz or 10 mHz, signed mg samples', () {
      final m = MotionPacket.parse(motion(7, [(980, -60, 150), (-1000, 32767, -32768)], uptime: 42))!;
      expect((m.sequence, m.uptimeMs, m.rateHz), (7, 42, 12.5));
      expect([for (final s in m.samples) (s.x, s.y, s.z)], [(980, -60, 150), (-1000, 32767, -32768)]);
      expect(MotionPacket.parse(motion(0, const [], rate: 0xA8A0))!.rateHz, closeTo(104.0, 1e-9));
      expect(MotionPacket.parse(Uint8List(3)), isNull);
    });
  });

  group('sequence gaps', () {
    test('counts missed notifications, across the wrap at 65535', () {
      final g = SequenceGaps();
      for (final s in [10, 11, 12, 15, 15, 16]) {
        g.add(s);
      }
      expect((g.missed, g.gaps), (2, 1), reason: '13 and 14 missing; a repeat of 15 is not a gap');

      final w = SequenceGaps();
      for (final s in [65533, 65534, 65535, 0, 1]) {
        w.add(s);
      }
      expect((w.missed, w.gaps), (0, 0), reason: '65535 to 0 is the next number');

      final w2 = SequenceGaps()
        ..add(65534)
        ..add(2);
      expect((w2.missed, w2.gaps), (3, 1), reason: '65535, 0 and 1 missing');

      final r = SequenceGaps()
        ..add(30000)
        ..add(5);
      expect((r.missed, r.gaps), (0, 0), reason: 'a jump back is the pod restarting its counter');
    });
  });

  group('session file', () {
    test('binary to stats to CSV, round trip; a cut-off last record is left out', () async {
      final store = RawStore(Directory('${tmp.path}/raw'));
      final w = store.open(1762750000000)
        ..add(RawChar.ppgData, ppg(65535, [(0x01, 100), (0x02, 200), (0x01, 101)], uptime: 5000), 1762750000100)
        ..add(RawChar.motionData, motion(9, [(1, 2, 3), (4, 5, 6)], uptime: 5010), 1762750000150)
        ..add(RawChar.ppgData, ppg(1, [(0x01, 102)], uptime: 5080), 1762750000300)
        ..close();
      final bytes = w.file.readAsBytesSync();
      expect(bytes.sublist(0, 4), RawFile.magic);

      final s = RawFile.stats(bytes);
      expect(s.packets, 3);
      expect(s.ppgSamples, {'green': 3, 'ir': 1});
      expect(s.motionSamples, 2);
      expect((s.ppgGaps.missed, s.ppgGaps.gaps), (1, 1), reason: '65535 then 1: 0 is missing');
      expect((s.startMs, s.endMs), (1762750000100, 1762750000300));

      final list = await store.list();
      expect(list.single.id, w.id);
      expect(list.single.ppgSamples, {'green': 3, 'ir': 1});
      expect(list.single.ppgMissed, 1);
      expect(list.single.duration, const Duration(milliseconds: 200));

      expect(ppgCsv(bytes).trimRight().split('\n'), [
        ppgCsvColumns.join(','),
        '2025-11-10T04:46:40.100Z,1762750000100,5000.0,65535,green,0x01,0,100,1,25',
        '2025-11-10T04:46:40.100Z,1762750000100,5000.0,65535,ir,0x02,0,200,1,25',
        '2025-11-10T04:46:40.100Z,1762750000100,5040.0,65535,green,0x01,0,101,1,25',
        '2025-11-10T04:46:40.300Z,1762750000300,5080.0,1,green,0x01,0,102,1,25',
      ]);
      expect(motionCsv(bytes).trimRight().split('\n'), [
        motionCsvColumns.join(','),
        '2025-11-10T04:46:40.150Z,1762750000150,5010.0,9,1,2,3,12.50',
        '2025-11-10T04:46:40.150Z,1762750000150,5090.0,9,4,5,6,12.50',
      ]);

      final cut = Uint8List.sublistView(bytes, 0, bytes.length - 2);
      expect(RawFile.stats(cut).packets, 2);

      // The export: two CSVs and the raw file.
      final files = await writeSessionExport(list.single, Directory('${tmp.path}/out'));
      expect([for (final f in files) f.uri.pathSegments.last], ['${w.id}-ppg.csv', '${w.id}-motion.csv', '${w.id}.bin']);
      expect(files.last.readAsBytesSync(), bytes);
    });

    test('a session the app never closed is summarised from its bytes', () async {
      final store = RawStore(Directory('${tmp.path}/raw'));
      final w = store.open(1)..add(RawChar.ppgData, ppg(0, [(0x01, 1)]), 10);
      w.flush();
      File(w.file.path.replaceAll('.bin', '.json')).deleteSync();
      final s = (await store.list()).single;
      expect(s.ppgSamples, {'green': 1});
    });

    test('a session with no packets leaves no file', () {
      final store = RawStore(Directory('${tmp.path}/raw'));
      store.open(1).close();
      expect(store.usedBytes(), 0);
    });
  });

  group('size limit', () {
    test('a write that would pass the limit is refused and nothing is deleted', () {
      final store = RawStore(Directory('${tmp.path}/raw'), capBytes: 150);
      final old = store.open(1)
        ..add(RawChar.ppgData, ppg(0, [(0x01, 1)]), 1)
        ..close();
      final w = store.open(2);
      final p = ppg(1, [for (var i = 0; i < 20; i++) (0x01, i)]);
      w.add(RawChar.ppgData, p, 2);
      expect(() => w.add(RawChar.ppgData, p, 3), throwsA(isA<RawCapReached>()));
      w.close();
      expect(old.file.existsSync(), isTrue, reason: 'never deleted silently');
      expect(store.usedBytes(), lessThanOrEqualTo(150));
      expect(RawFile.stats(w.file.readAsBytesSync()).packets, 1);
    });

    test('the recorder stops at the limit, turns the streams off and says so; deleting lets it record again', () async {
      final source = _Source();
      final store = RawStore(Directory('${tmp.path}/raw'), capBytes: 300);
      final c = ProviderContainer(overrides: [rawSourceProvider.overrideWithValue(source), rawStoreProvider.overrideWith((_) async => store)]);
      addTearDown(c.dispose);
      c.read(rawRecorderProvider);
      c.read(appStateProvider.notifier).toggle(rawRecordingKey);
      source.connect(true);
      await Future<void>.delayed(RawRecorder.startDelay + const Duration(milliseconds: 100));
      expect(c.read(rawRecorderProvider).recording, isTrue);
      expect(source.starts, 1);

      final p = ppg(0, [for (var i = 0; i < 20; i++) (0x01, i)]);
      for (var i = 0; i < 6; i++) {
        source.send(RawChar.ppgData, p);
        await Future<void>.delayed(Duration.zero);
      }
      final st = c.read(rawRecorderProvider);
      expect(st.capReached, isTrue);
      expect(st.recording, isFalse);
      expect(source.stops, 1, reason: 'streams off, to spare the battery');
      expect(store.usedBytes(), lessThanOrEqualTo(300));
      expect((await store.list()).single.ppgTotal, greaterThan(0), reason: 'kept, not deleted');

      await c.read(rawRecorderProvider.notifier).deleteAll();
      await Future<void>.delayed(Duration.zero);
      expect(c.read(rawRecorderProvider).capReached, isFalse);
      expect(c.read(rawRecorderProvider).recording, isTrue, reason: 'still connected and on: records again');
    });

    test('one session per connection: a drop ends it, the next connect starts another', () async {
      final source = _Source();
      final store = RawStore(Directory('${tmp.path}/raw'));
      final c = ProviderContainer(overrides: [rawSourceProvider.overrideWithValue(source), rawStoreProvider.overrideWith((_) async => store)]);
      addTearDown(c.dispose);
      c.read(rawRecorderProvider);
      c.read(appStateProvider.notifier).toggle(rawRecordingKey);
      for (var k = 0; k < 2; k++) {
        source.connect(true);
        await Future<void>.delayed(RawRecorder.startDelay + const Duration(milliseconds: 100));
        source.send(RawChar.motionData, motion(k, [(1, 2, 3)]));
        await Future<void>.delayed(const Duration(milliseconds: 5));
        source.connect(false);
        await Future<void>.delayed(const Duration(milliseconds: 5));
      }
      expect((await store.list()).length, 2);
      expect(source.starts, 2);

      c.read(appStateProvider.notifier).toggle(rawRecordingKey);
      source.connect(true);
      await Future<void>.delayed(RawRecorder.startDelay + const Duration(milliseconds: 100));
      expect(c.read(rawRecorderProvider).recording, isFalse, reason: 'off: nothing recorded');
    });
  });

  group('PPG Control shared with the fit guide', () {
    test('bits are ORed, and leaving the fit guide keeps a recording running', () async {
      final host = _Host();
      final link = PodLink(host: host);
      await link.startRawStreams();
      expect(host.writes.last, 0x03);
      link.startFit();
      await Future<void>.delayed(Duration.zero);
      expect(host.writes.last, 0x07);
      await link.renewFit();
      expect(host.writes.last, 0x07);
      await link.stopFit();
      expect(host.writes.last, 0x03, reason: 'the fit bits go, the recording bits stay');
      expect(host.notify.where((n) => n.$1 == Gatt.ppgData && !n.$2), isEmpty, reason: 'PPG notifications stay on');
      await link.stopRawStreams();
      expect(host.writes.last, 0x00);
      expect(host.notify.last, (Gatt.ppgData, false));
    });

    test('stopping a recording keeps the fit guide running', () async {
      final host = _Host();
      final link = PodLink(host: host)..startFit();
      await link.startRawStreams();
      expect(host.writes.last, 0x07);
      await link.stopRawStreams();
      expect(host.writes.last, 0x05);
      expect(host.notify.where((n) => n.$1 == Gatt.ppgData && !n.$2), isEmpty);
    });
  });

  group('demo signal', () {
    test('synthetic packets are in the wire format and decode', () {
      final rng = math.Random(1);
      final p = PpgPacket.parse(demoPpgPacket(3, 4000, rng))!;
      expect((p.sequence, p.rateHz, p.mode, p.items.length), (3, 25, 1, 32));
      expect(p.samples().every((s) => s.slot == 1), isTrue);
      final m = MotionPacket.parse(demoMotionPacket(4, 4000, rng))!;
      expect((m.sequence, m.rateHz, m.samples.length), (4, 12.5, 12));
    });
  });

  group('On this phone', () {
    test('sizes by kind from the data and cache folders', () {
      final support = Directory('${tmp.path}/support')..createSync();
      final cache = Directory('${tmp.path}/cache')..createSync();
      File('${support.path}/epochs.bin').writeAsBytesSync(Uint8List(1600));
      File('${support.path}/epochs.bin.cursor').writeAsStringSync('12');
      Directory('${support.path}/pod_spool').createSync();
      File('${support.path}/pod_spool/a.bin').writeAsBytesSync(Uint8List(300));
      Directory('${support.path}/raw').createSync();
      File('${support.path}/raw/raw-1.bin').writeAsBytesSync(Uint8List(5000));
      File('${support.path}/raw/raw-1.json').writeAsStringSync('{}');
      Directory('${support.path}/hive').createSync();
      File('${support.path}/hive/settings.hive').writeAsBytesSync(Uint8List(700));
      Directory('${cache.path}/atrium-export').createSync();
      File('${cache.path}/atrium-export/a.csv').writeAsBytesSync(Uint8List(50));
      File('${cache.path}/unrelated.tmp').writeAsBytesSync(Uint8List(9999));

      final u = StorageUsage.measure(support, cache);
      expect((u.readings, u.waiting, u.raw, u.app, u.exports), (1602, 300, 5002, 700, 50));
      expect(u.total, 1602 + 300 + 5002 + 700 + 50);
      expect(StorageUsage.measure(Directory('${tmp.path}/none'), null).total, 0);
    });

    test('sizes read as phones show them', () {
      expect(formatBytes(0), '0 bytes');
      expect(formatBytes(1), '1 byte');
      expect(formatBytes(1500), '1.5 KB');
      expect(formatBytes(182 * 1000 * 1000), '182 MB');
      expect(formatBytes(2400000000), '2.4 GB');
    });
  });

  test('session lines', () {
    expect(durationLabel(const Duration(seconds: 40)), '40 s');
    expect(durationLabel(const Duration(minutes: 192)), '3 h 12 min');
    final s = RawSession(
      id: 'raw-1',
      file: File('x'),
      bytes: 1,
      startMs: 0,
      endMs: 2520000,
      ppgSamples: const {'green': 63000},
      motionSamples: 31500,
      ppgMissed: 2,
      ppgGaps: 1,
    );
    expect(sessionLine(s), '42 min · green 63,000 · motion 31,500 · 2 missed packets in 1 gap');
  });
}
