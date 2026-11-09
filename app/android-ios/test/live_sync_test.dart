// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

/// Rev 13 sync (firmware audit 2026-11-08): Live Epoch 0x0504, the head-jump rule, and the full sync against a
/// fake pod that serves the ML reference log (8 pages).
class _FakePod extends PodHostApi {
  _FakePod(this.pages, {this.refuse});
  final Uint8List pages;
  final String? refuse;
  late PodLink link;
  final notified = <int>[];

  @override
  Future<Uint8List> read(int characteristic) async {
    // Log Info: oldest 0, head 7.
    final b = ByteData(18)
      ..setUint32(0, 0, Endian.little)
      ..setUint32(4, 7, Endian.little);
    return b.buffer.asUint8List();
  }

  @override
  Future<void> setNotify(int characteristic, bool enabled) async => notified.add(characteristic);

  @override
  Future<void> syncLog(int fromPage) async {
    if (refuse != null) throw PlatformException(code: refuse!, message: 'Log read refused');
    // The native host hands Dart the complete pages after the pod's 0x90 end indication.
    link.onLogPages(fromPage, Uint8List.sublistView(pages, fromPage * pageSize));
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final log = Uint8List.fromList(File('test/fixtures/phone/phone_log.bin').readAsBytesSync());
  final ref = decodeBlocks(log);
  late Directory dir;
  setUp(() => dir = Directory.systemTemp.createTempSync('atrium_sync'));
  tearDown(() => dir.deleteSync(recursive: true));

  Uint8List live(int t, int head, Uint8List raw) {
    final b = ByteData(20)
      ..setUint32(0, t, Endian.little)
      ..setUint32(4, head, Endian.little);
    final out = b.buffer.asUint8List()..setRange(8, 20, raw);
    return out;
  }

  test('Live Epoch: time, head page and the record as logged', () {
    final e = ref[100];
    final got = LiveEpoch.parse(live(e.time, 6, e.raw!))!;
    expect(got.epoch.time, e.time);
    expect(got.epoch.isUptime, isFalse);
    expect(got.headPage, 6);
    expect(got.epoch.raw, e.raw);
    expect(got.epoch.epoch.pulseBpm, e.epoch.pulseBpm);
    expect(got.epoch.epoch.activityMg, e.epoch.activityMg);
    final up = LiveEpoch.parse(live(uptimeBit | 5000, 6, e.raw!))!;
    expect(up.epoch.isUptime, isTrue);
    expect(up.epoch.time, 5000);
    expect(LiveEpoch.parse(Uint8List(19)), isNull);
  });

  test('a head page more than one past the cursor needs a sync', () {
    expect(PodLink.needsSync(headPage: 5, cursor: 5), isFalse);
    expect(PodLink.needsSync(headPage: 6, cursor: 5), isFalse); // the head moved on by one: live epochs cover it
    expect(PodLink.needsSync(headPage: 7, cursor: 5), isTrue);
  });

  test('full sync stores every epoch once, and the cursor is the head page', () async {
    final pod = _FakePod(log);
    final link = PodLink(host: pod);
    pod.link = link;
    final store = EpochStore(File('${dir.path}/epochs.bin'));
    expect(await link.sync(store), ref.length);
    expect(store.syncedToPage, 7);
    expect(link.waitingForPages, isFalse);
    // A second sync re-reads the head page and adds nothing new.
    expect(await link.sync(store), 0);
    expect(store.count, ref.length);
  });

  test('a refused read leaves nothing waiting; busy is skipped, out of range is an error', () async {
    final store = EpochStore(File('${dir.path}/epochs.bin'));
    final busy = _FakePod(log, refuse: 'log_status_2');
    final l1 = PodLink(host: busy);
    busy.link = l1;
    expect(await l1.sync(store), 0);
    expect(l1.waitingForPages, isFalse);
    final range = _FakePod(log, refuse: 'log_status_3');
    final l2 = PodLink(host: range);
    range.link = l2;
    await expectLater(l2.sync(store), throwsA(isA<PlatformException>()));
    expect(l2.waitingForPages, isFalse);
  });

  test('live epochs subscribe to 0x0504', () async {
    final pod = _FakePod(log);
    final link = PodLink(host: pod);
    pod.link = link;
    final got = <LiveEpoch>[];
    final sub = link.liveEpochs().listen(got.add);
    link.onNotify(Gatt.liveEpoch, live(ref[0].time, 7, ref[0].raw!));
    link.onNotify(Gatt.chargeState, Uint8List(19));
    await Future<void>.delayed(Duration.zero);
    expect(pod.notified, contains(Gatt.liveEpoch));
    expect(got.length, 1);
    expect(got.single.epoch.time, ref[0].time);
    await sub.cancel();
  });
}
