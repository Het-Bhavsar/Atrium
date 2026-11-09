// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:atrium/features/pod/data/spool.dart';
import 'package:atrium/features/pod/domain/link_machine.dart';
import 'package:atrium/features/pod/domain/sync_runner.dart';
import 'package:flutter_test/flutter_test.dart';

/// The pod connection and the log sync as state machines (the OTA one is in firmware_update_test), and background
/// sync's spool files, taken in with the ML reference log.
void main() {
  group('connection', () {
    test('pair, drop, reconnect', () {
      var p = LinkPhase.noPod;
      p = nextLink(p, LinkEvent.connecting);
      expect(p, LinkPhase.connecting);
      p = nextLink(p, LinkEvent.connected);
      expect(p, LinkPhase.connected);
      p = nextLink(p, LinkEvent.dropped);
      expect(p, LinkPhase.waiting);
      expect(shouldReconnect(p), isTrue);
      expect(nextLink(p, LinkEvent.connecting), LinkPhase.connecting);
    });

    test('an update owns the link: drops and connects change nothing until it is taken back', () {
      var p = nextLink(LinkPhase.connected, LinkEvent.handOver);
      expect(p, LinkPhase.handedOver);
      for (final e in [LinkEvent.dropped, LinkEvent.connected, LinkEvent.connecting, LinkEvent.bluetoothOn, LinkEvent.permissionGranted]) {
        expect(nextLink(p, e), LinkPhase.handedOver, reason: '$e');
      }
      expect(linkUsable(p), isFalse, reason: 'no sync or writes while MCUmgr has the pod');
      expect(shouldReconnect(p), isFalse);
      p = nextLink(p, LinkEvent.takeBack);
      expect(p, LinkPhase.connecting);
      expect(nextLink(LinkPhase.handedOver, LinkEvent.forgotten), LinkPhase.noPod, reason: 'log out still wins');
    });

    test('permission and Bluetooth stop retries, and fixing them resumes', () {
      expect(nextLink(LinkPhase.connected, LinkEvent.permissionDenied), LinkPhase.needsPermission);
      expect(nextLink(LinkPhase.needsPermission, LinkEvent.connecting), LinkPhase.needsPermission);
      expect(nextLink(LinkPhase.needsPermission, LinkEvent.dropped), LinkPhase.needsPermission);
      expect(nextLink(LinkPhase.needsPermission, LinkEvent.permissionGranted), LinkPhase.waiting);
      expect(nextLink(LinkPhase.waiting, LinkEvent.bluetoothOff), LinkPhase.bluetoothOff);
      expect(shouldReconnect(LinkPhase.bluetoothOff), isFalse);
      expect(nextLink(LinkPhase.bluetoothOff, LinkEvent.bluetoothOn), LinkPhase.waiting);
      expect(nextLink(LinkPhase.noPod, LinkEvent.dropped), LinkPhase.noPod, reason: 'no pod: nothing to wait for');
    });

    test('every phase has a way out, and forgotten always ends at noPod', () {
      for (final p in LinkPhase.values) {
        expect(LinkEvent.values.any((e) => allowed(p, e)), isTrue, reason: '$p is a dead end');
        expect(nextLink(p, LinkEvent.forgotten), LinkPhase.noPod);
      }
    });

    test('reconnect waits back off and stop at 2 minutes', () {
      expect(reconnectDelay(0), const Duration(seconds: 1));
      expect([for (var i = 0; i < 8; i++) reconnectDelay(i).inSeconds], [1, 2, 5, 10, 30, 60, 120, 120]);
      for (var i = 1; i < 20; i++) {
        expect(reconnectDelay(i) >= reconnectDelay(i - 1), isTrue);
      }
    });
  });

  group('sync', () {
    test('requests during a sync fold into one more run, and the outcome is kept', () async {
      final gate = Completer<void>();
      var calls = 0;
      final seen = <SyncStatus>[];
      final r = SyncRunner(
        () async {
          calls++;
          if (calls == 1) await gate.future;
          return 10;
        },
        onChange: seen.add,
        clock: () => DateTime.utc(2026, 11, 10, 8),
      );
      final a = r.request();
      final b = r.request();
      final c = r.request();
      expect(r.status.syncing, isTrue);
      r.progress(3, 8);
      expect(r.status.progress, closeTo(3 / 8, 1e-9));
      gate.complete();
      expect(await a, 20, reason: 'the first run and the one more it asked for');
      expect(await b, 20);
      expect(await c, 20);
      expect(calls, 2, reason: 'three requests, two runs');
      expect(r.status.phase, SyncPhase.idle);
      expect(r.status.lastSync, DateTime.utc(2026, 11, 10, 8));
      expect(r.status.lastAdded, 10);
    });

    test('a failed sync keeps the last good time and says why; the next request runs again', () async {
      var fail = true;
      final r = SyncRunner(() async => fail ? throw Exception('pod busy') : 3, onChange: (_) {}, clock: () => DateTime.utc(2026, 11, 10, 9));
      r.restore(DateTime.utc(2026, 11, 9));
      await r.request();
      expect(r.status.phase, SyncPhase.failed);
      expect(r.status.error, contains('pod busy'));
      expect(r.status.lastSync, DateTime.utc(2026, 11, 9));
      fail = false;
      expect(await r.request(), 3);
      expect(r.status.phase, SyncPhase.idle);
      expect(r.status.error, isNull);
    });

    test('cancel (provider disposed) stops further runs and updates', () async {
      final gate = Completer<void>();
      var changes = 0;
      final r = SyncRunner(() async {
        await gate.future;
        return 1;
      }, onChange: (_) => changes++);
      final a = r.request();
      final before = changes;
      unawaited(r.request());
      r.cancel();
      gate.complete();
      await a;
      expect(r.runs, 1, reason: 'the queued run is dropped');
      expect(changes, before, reason: 'nothing is reported after dispose');
      expect(await r.request(), 0);
    });

    test('idle waits for the run in flight, so an update never starts mid-sync', () async {
      final gate = Completer<void>();
      final r = SyncRunner(() async {
        await gate.future;
        return 1;
      }, onChange: (_) {});
      unawaited(r.request());
      var done = false;
      final w = r.idle().then((_) => done = true);
      await pumpEventQueue();
      expect(done, isFalse);
      gate.complete();
      await w;
      expect(done, isTrue);
    });
  });

  group('pod card', () {
    test('derived from the session: live while connected, the sync age otherwise', () {
      final now = DateTime.utc(2026, 11, 10, 12);
      final status = LiveStatus.parse(Uint8List(19)
        ..[4] = 0x08
        ..[5] = 74)!;
      final on = podStatusOf(
        PodSessionState(phase: LinkPhase.connected, status: status, sync: SyncStatus(lastSync: now.subtract(const Duration(minutes: 3)))),
        now,
      );
      expect((on.connected, on.batteryPercent, on.label, on.lastSync), (true, 70, 'On skin', '3 min ago'));
      final off = podStatusOf(PodSessionState(phase: LinkPhase.waiting, status: status, sync: SyncStatus(lastSync: now.subtract(const Duration(hours: 17)))), now);
      expect((off.connected, off.hoursHeld, off.lastSync), (false, 17, '17 h ago'));
      expect(podStatusOf(const PodSessionState(phase: LinkPhase.bluetoothOff), now).line, 'Bluetooth is off');
      expect(podStatusOf(const PodSessionState(phase: LinkPhase.handedOver), now).line, contains('pod software'));
    });
  });

  group('background spool', () {
    final log = Uint8List.fromList(File('test/fixtures/phone/phone_log.bin').readAsBytesSync());
    final ref = decodeBlocks(log);
    late Directory dir;
    setUp(() => dir = Directory.systemTemp.createTempSync('atrium_spool'));
    tearDown(() => dir.deleteSync(recursive: true));

    test('native pages decode with the app codec; the cursor and the sync time move on; files are removed', () async {
      final spool = Directory('${dir.path}/${Spool.dirName}')..createSync();
      // Two background syncs: pages 0-3, then 3-7 (the head page is read again).
      File('${spool.path}/1762740000-0.bin').writeAsBytesSync(
        Spool.encode(firstPage: 0, podClock: 1762740000, phoneUnix: 1762740000, pages: Uint8List.sublistView(log, 0, 4 * pageSize)),
      );
      File('${spool.path}/1762750000-3.bin').writeAsBytesSync(
        Spool.encode(firstPage: 3, podClock: 1762750000, phoneUnix: 1762750000, pages: Uint8List.sublistView(log, 3 * pageSize)),
      );
      File('${spool.path}/1762760000-9.tmp').writeAsBytesSync([1, 2, 3]);
      final store = EpochStore(File('${dir.path}/epochs.bin'));
      final (added, at) = await Spool.ingest(spool, store);
      expect(added, ref.length, reason: 'every reading once, the overlap page deduplicated');
      expect(store.syncedToPage, 7);
      expect(at, DateTime.fromMillisecondsSinceEpoch(1762750000 * 1000));
      expect(store.lastSyncAt, at);
      expect(spool.listSync().map((f) => f.path.split('/').last), ['1762760000-9.tmp'], reason: 'a half-written file waits');
      // Reopened from disk, the readings and the cursor are there.
      final again = EpochStore(File('${dir.path}/epochs.bin'));
      await again.load();
      expect((again.count, again.syncedToPage), (ref.length, 7));
    });

    test('a pod that had lost its clock is rebased like a foreground sync', () {
      // Shift the last run onto uptime, as a pod that rebooted would log it.
      const uptimeNow = 50000;
      final last = ref.last.time;
      final b = Spool.encode(firstPage: 0, podClock: uptimeNow | uptimeBit, phoneUnix: last + 30, pages: log);
      final batch = Spool.decode(b)!;
      expect(batch.epochs.length, ref.length);
      expect(batch.cursor, 7);
      expect(Spool.decode(Uint8List(8)), isNull);
      expect(Spool.decode(Uint8List(32)), isNull, reason: 'wrong magic');
    });

    test('a spool without whole pages leaves the cursor alone', () async {
      final spool = Directory('${dir.path}/${Spool.dirName}')..createSync();
      File('${spool.path}/1-0.bin').writeAsBytesSync(Spool.encode(firstPage: 5, podClock: 0, phoneUnix: 1762740000, pages: Uint8List(100)));
      final store = EpochStore(File('${dir.path}/epochs.bin'));
      await store.setCursor(2);
      await Spool.ingest(spool, store);
      expect(store.syncedToPage, 2);
      expect(PodLink.nextCursor(first: 5, bytes: 100, cursor: 2), 2);
    });
  });
}
