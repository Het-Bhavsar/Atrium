// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:typed_data';

import 'package:atrium/features/device/data/mcumgr_transport.dart';
import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:flutter_test/flutter_test.dart';

/// Pod software update (#21): the image checks, the release pick, the battery rule and the state machine, against
/// fakes for GitHub, MCUmgr and the app's pod link. Success must only follow a restart that reports the new version.

/// A minimal MCUboot image: 32-byte header, [body] bytes, an optional protected TLV area, then the TLV area with the
/// SHA-256 of header, body and protected area (as imgtool writes it). [corrupt] flips a body byte after hashing.
Uint8List image(
  PodVersion v, {
  int body = 64,
  int magic = McubootImage.magic,
  int tlv = McubootImage.tlvMagic,
  bool withHash = true,
  bool corrupt = false,
  bool protectedTlv = false,
}) {
  const prot = 8; // info header + one empty entry
  final protSize = protectedTlv ? prot : 0;
  final tlvSize = 4 + (withHash ? 36 : 0);
  final b = ByteData(32 + body + protSize + tlvSize)
    ..setUint32(0, magic, Endian.little)
    ..setUint16(8, 32, Endian.little)
    ..setUint16(10, protSize, Endian.little)
    ..setUint32(12, body, Endian.little)
    ..setUint8(20, v.major)
    ..setUint8(21, v.minor)
    ..setUint16(22, v.patch, Endian.little)
    ..setUint32(24, 7, Endian.little);
  for (var i = 0; i < body; i++) {
    b.setUint8(32 + i, (i * 31 + v.minor) & 0xFF);
  }
  var o = 32 + body;
  if (protectedTlv) {
    b
      ..setUint16(o, McubootImage.protectedTlvMagic, Endian.little)
      ..setUint16(o + 2, prot, Endian.little)
      ..setUint16(o + 4, 0x50, Endian.little)
      ..setUint16(o + 6, 0, Endian.little);
    o += prot;
  }
  b
    ..setUint16(o, protectedTlv ? McubootImage.tlvMagic : tlv, Endian.little)
    ..setUint16(o + 2, tlvSize, Endian.little);
  final bytes = b.buffer.asUint8List();
  if (withHash) {
    b
      ..setUint16(o + 4, McubootImage.tlvSha256, Endian.little)
      ..setUint16(o + 6, 32, Endian.little);
    bytes.setRange(o + 8, o + 40, sha256Of(Uint8List.sublistView(bytes, 0, o)));
  }
  if (corrupt) bytes[40] ^= 0xFF;
  return bytes;
}

Uint8List hashOf(Uint8List img) => McubootImage.parse(img).sha256;

const v010 = PodVersion(0, 1, 0);
const v020 = PodVersion(0, 2, 0);

FirmwareRelease release(PodVersion v) =>
    FirmwareRelease(version: v, tag: 'v$v', assetName: 'atrium-pod-$v.bin', url: Uri.parse('https://github.com/x/atrium-pod-$v.bin'), size: 0);

class FakeSource implements FirmwareSource {
  FakeSource({this.rel, this.bytes, this.error});
  FirmwareRelease? rel;
  Uint8List? bytes;
  Object? error;
  int downloads = 0;
  Completer<void>? gate;

  @override
  Future<FirmwareRelease?> latest() async {
    if (error != null) throw error!;
    return rel;
  }

  @override
  Future<Uint8List> download(FirmwareRelease r, {required void Function(int got, int total) onProgress, required bool Function() isCancelled}) async {
    downloads++;
    final b = bytes!;
    onProgress(b.length ~/ 2, b.length);
    if (gate != null) await gate!.future;
    if (isCancelled()) throw const OtaCancelled();
    onProgress(b.length, b.length);
    return b;
  }
}

class FakeDfu implements DfuTransport {
  FakeDfu({this.fail, this.dropTimes = 0, this.slotScript, this.oldHash});
  final String? fail;

  /// Uploads that lose the link before the next one goes through.
  int dropTimes;

  /// Slot lists returned in order (the last one repeats); null plays a pod that confirms the new image at once.
  final List<List<PodSlot>?>? slotScript;
  final Uint8List? oldHash;
  String? device;
  Uint8List? hash;
  final progress = <double>[];
  Completer<void>? gate;
  bool cancelled = false;
  int uploads = 0;
  int slotReads = 0;

  @override
  Future<void> upload(String deviceId, Uint8List image, Uint8List hash, {required void Function(int sent, int total) onProgress, void Function()? onReset}) async {
    uploads++;
    device = deviceId;
    this.hash = hash;
    onProgress(image.length ~/ 4, image.length);
    if (gate != null) await gate!.future;
    if (cancelled) throw const OtaCancelled();
    if (dropTimes > 0) {
      dropTimes--;
      throw const OtaLinkLost('dropped');
    }
    if (fail != null) throw OtaFailure(fail!);
    onProgress(image.length, image.length);
    onReset?.call();
  }

  @override
  Future<List<PodSlot>> slots(String deviceId) async {
    final script = slotScript;
    final i = slotReads++;
    if (script != null) {
      final s = script[i < script.length ? i : script.length - 1];
      if (s == null) throw Exception('not reachable');
      return s;
    }
    final old = PodSlot(slot: 0, hash: oldHash ?? Uint8List(32), active: true, confirmed: true);
    if (hash == null) return [old];
    return [PodSlot(slot: 0, hash: hash!, active: true, confirmed: true), PodSlot(slot: 1, hash: oldHash ?? Uint8List(32))];
  }

  @override
  Future<void> cancel() async {
    cancelled = true;
    gate?.complete();
  }
}

class FakePod implements PodHandover {
  FakePod({this.version = v010, this.after, this.block});
  PodVersion? version;

  /// What the pod reports after the restart (null: not reached).
  PodVersion? after;
  String? block;
  int releases = 0;
  int reclaims = 0;

  @override
  Future<PodVersion?> readVersion() async => version;

  @override
  String? blocker() => block;

  @override
  Future<String> release() async {
    releases++;
    return 'pod-1';
  }

  @override
  Future<PodVersion?> reclaim(Duration timeout) async {
    reclaims++;
    if (after != null) version = after;
    return after;
  }
}

FirmwareUpdateFlow flow(FakeSource s, FakeDfu d, FakePod p, [List<OtaState>? seen]) =>
    FirmwareUpdateFlow(source: s, dfu: d, pod: p, onChange: seen?.add, wait: (_) async {});

void main() {
  group('versions', () {
    test('DIS, MCUmgr and tag forms parse to major.minor.patch', () {
      expect(PodVersion.tryParse('0.1.0+41f1d65'), v010);
      expect(PodVersion.tryParse('0.1.0+41f1d65-dirty'), v010);
      expect(PodVersion.tryParse('v0.2.0'), v020);
      expect(PodVersion.tryParse('0.2.0.0'), v020);
      expect(PodVersion.tryParse('EVT'), isNull);
      expect(PodVersion.tryParse(null), isNull);
    });

    test('compare by number, not by text', () {
      expect(const PodVersion(0, 10, 0) > const PodVersion(0, 9, 9), isTrue);
      expect(const PodVersion(1, 0, 0) > const PodVersion(0, 99, 99), isTrue);
      expect(v010 > v010, isFalse);
    });
  });

  group('image', () {
    test('a whole MCUboot image gives its version', () {
      final i = McubootImage.parse(image(v020));
      expect(i.version, v020);
      expect(i.build, 7);
      expect(McubootImage.parse(image(v020, protectedTlv: true)).version, v020, reason: 'the hash covers the protected TLVs');
      expect(McubootImage.parse(image(v020)).sha256, hasLength(32));
    });

    test('an image whose bytes do not match its SHA-256, or that has none, is refused', () {
      expect(() => McubootImage.parse(image(v020, corrupt: true)), throwsA(isA<FormatException>().having((e) => e.message, 'message', contains('damaged'))));
      expect(() => McubootImage.parse(image(v020, withHash: false)), throwsA(isA<FormatException>().having((e) => e.message, 'message', contains('hash'))));
    });

    test('other files, cut files and oversize files are refused before anything reaches the pod', () {
      expect(() => McubootImage.parse(image(v020, magic: 0x12345678)), throwsFormatException);
      expect(() => McubootImage.parse(Uint8List.sublistView(image(v020), 0, 60)), throwsFormatException);
      expect(() => McubootImage.parse(image(v020, tlv: 0xFFFF)), throwsFormatException);
      expect(() => McubootImage.parse(image(v020, body: maxImageBytes)), throwsFormatException);
      // Between the slot (216938 B) and the old limit (222105 B): refused before anything reaches the pod.
      expect(() => McubootImage.parse(image(v020, body: 217000)), throwsFormatException);
      expect(McubootImage.parse(image(v020, body: 213951 - 32 - 40)).version, v020, reason: "today's signed image fits");
      expect(() => McubootImage.parse(Uint8List(10)), throwsFormatException);
    });
  });

  group('release', () {
    Map<String, dynamic> gh(List<(String, String)> assets) => {
      'tag_name': 'v0.2.0',
      'body': ' Notes ',
      'published_at': '2026-11-09T10:00:00Z',
      'assets': [
        for (final (n, u) in assets) {'name': n, 'browser_download_url': u, 'size': 1234, 'digest': 'sha256:${'Ab' * 32}'},
      ],
    };

    test('picks atrium-pod-<version>.bin, the highest if several', () {
      final r = FirmwareRelease.fromGitHub(
        gh([
          ('atrium-pod-0.1.0.bin', 'https://github.com/a/0.1.0.bin'),
          ('atrium-pod-0.2.0.bin', 'https://github.com/a/0.2.0.bin'),
          ('atrium-case-0.9.0.bin', 'https://github.com/a/case.bin'),
          ('atrium-pod-0.2.0.zip', 'https://github.com/a/0.2.0.zip'),
        ]),
      )!;
      expect(r.version, v020);
      expect(r.assetName, 'atrium-pod-0.2.0.bin');
      expect(r.size, 1234);
      expect(r.notes, 'Notes');
      expect(r.published, DateTime.utc(2026, 11, 9, 10));
      expect(r.sha256, 'ab' * 32, reason: "GitHub's asset digest, lower case");
    });

    test('no pod image, or a link that is not https, is no release', () {
      expect(FirmwareRelease.fromGitHub(gh([('notes.txt', 'https://x/y')])), isNull);
      expect(FirmwareRelease.fromGitHub(gh([('atrium-pod-0.2.0.bin', 'http://github.com/a.bin')])), isNull);
      expect(FirmwareRelease.fromGitHub(const {'message': 'Not Found'}), isNull);
    });
  });

  group('battery rule (3.60 V off the dock)', () {
    test('docked always passes; off the dock needs a reading at or above 3.60 V', () {
      expect(updateBlocker(connected: true, docked: true), isNull);
      expect(updateBlocker(connected: true, docked: false, vsysMv: 3600), isNull);
      expect(updateBlocker(connected: true, docked: false, vsysMv: 3599), contains('too low'));
      expect(updateBlocker(connected: true, docked: false, vsysMv: 3900, batteryLow: true), contains('too low'));
      expect(updateBlocker(connected: true, docked: false), contains('Plinth'), reason: 'no voltage reading: ask for the dock');
      expect(updateBlocker(connected: false, docked: true), contains('Connect'));
    });

    test('Charge State bytes 6-7 carry VSYS', () {
      final v = Uint8List(19)
        ..[4] = 0x08
        ..[6] = 0x10
        ..[7] = 0x0E; // 0x0E10 = 3600 mV
      expect(LiveStatus.parse(v)!.vsysMv, 3600);
      expect(LiveStatus.parse(Uint8List(19))!.vsysMv, isNull);
      expect(LiveStatus.parse(v)!.toPodStatus(lastSync: 'x', hoursHeld: 0).vsysMv, 3600);
    });
  });

  group('state machine', () {
    test('every phase has its moves, and only verifying leads to done', () {
      for (final p in OtaPhase.values) {
        expect(otaTransitions.containsKey(p), isTrue, reason: '$p');
      }
      final intoDone = [
        for (final e in otaTransitions.entries)
          if (e.value.contains(OtaPhase.done)) e.key,
      ];
      expect(intoDone, [OtaPhase.verifying]);
    });

    test('check: newer release is available, same or older is up to date', () async {
      final f = flow(FakeSource(rel: release(v020)), FakeDfu(), FakePod());
      await f.check();
      expect(f.state.phase, OtaPhase.available);
      expect(f.state.current, v010);
      final g = flow(FakeSource(rel: release(v010)), FakeDfu(), FakePod());
      await g.check();
      expect(g.state.phase, OtaPhase.upToDate);
    });

    test('check: no release, or no network, is a failed check with a reason', () async {
      final none = flow(FakeSource(), FakeDfu(), FakePod());
      await none.check();
      expect(none.state.phase, OtaPhase.failed);
      expect(none.state.message, contains('No pod software'));
      final offline = flow(FakeSource(error: Exception('socket')), FakeDfu(), FakePod());
      await offline.check();
      expect(offline.state.phase, OtaPhase.failed);
      expect(offline.state.message, contains('internet'));
    });

    test('the full path ends in done only once the restarted pod reports the new version', () async {
      final seen = <OtaState>[];
      final pod = FakePod(after: v020);
      final dfu = FakeDfu();
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), dfu, pod, seen);
      await f.check();
      await f.start();
      expect(f.history, [
        OtaPhase.checking,
        OtaPhase.available,
        OtaPhase.downloading,
        OtaPhase.handingOver,
        OtaPhase.uploading,
        OtaPhase.restarting,
        OtaPhase.confirming,
        OtaPhase.verifying,
        OtaPhase.done,
      ]);
      expect(dfu.hash, hashOf(image(v020)), reason: 'MCUmgr names the image by its verified hash');
      expect(f.state.current, v020);
      expect(dfu.device, 'pod-1', reason: 'MCUmgr gets the id the app link let go');
      expect((pod.releases, pod.reclaims), (1, 1));
      final uploads = [
        for (final s in seen)
          if (s.phase == OtaPhase.uploading) s.progress,
      ];
      expect(uploads, containsAllInOrder([0.25, 1.0]), reason: 'progress comes from the upload callbacks');
    });

    test('a pod that comes back on the old version is a failure, not a success', () async {
      final h = hashOf(image(v020));
      // The slot list says confirmed, but the reconnected pod reports the old version: trust neither alone.
      final dfu = FakeDfu(
        slotScript: [
          null,
          [PodSlot(slot: 0, hash: h, active: true, confirmed: true)],
        ],
      );
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), dfu, FakePod(after: v010));
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.failed);
      expect(f.state.message, contains("didn't take"));
      expect(f.history, isNot(contains(OtaPhase.done)));
    });

    test('a pod not reached after the restart is not called updated', () async {
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), FakeDfu(), FakePod());
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.failed);
      expect(f.state.message, contains("isn't confirmed"));
    });

    test('the pod never confirms the image itself: done waits for its health check, a revert is a failure', () async {
      final h = hashOf(image(v020));
      final old = Uint8List(32);
      final trial = [PodSlot(slot: 0, hash: h, active: true), PodSlot(slot: 1, hash: old, confirmed: true)];
      final confirmed = [PodSlot(slot: 0, hash: h, active: true, confirmed: true), PodSlot(slot: 1, hash: old)];
      final reverted = [PodSlot(slot: 0, hash: old, active: true, confirmed: true), PodSlot(slot: 1, hash: h)];
      final ok = FakeDfu(slotScript: [null, null, trial, trial, confirmed]);
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), ok, FakePod(after: v020));
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.done);
      expect(ok.slotReads, 5, reason: 'read before the upload, then until confirmed');

      final pod = FakePod(after: v010);
      final g = flow(FakeSource(rel: release(v020), bytes: image(v020)), FakeDfu(slotScript: [null, trial, reverted]), pod);
      await g.check();
      await g.start();
      expect(g.state.phase, OtaPhase.failed);
      expect(g.state.message, allOf(contains("pod's own check"), contains('0.1.0')));
      expect(pod.reclaims, 1, reason: 'the app link comes back after a revert');
    });

    test('slots never readable: the version check waits past the 60 s health check', () async {
      final waits = <Duration>[];
      final dfu = FakeDfu(slotScript: [null]);
      final f = FirmwareUpdateFlow(
        source: FakeSource(rel: release(v020), bytes: image(v020)),
        dfu: dfu,
        pod: FakePod(after: v020),
        wait: (d) async => waits.add(d),
      );
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.done);
      final waited = waits.fold(Duration.zero, (a, b) => a + b);
      expect(waited >= const Duration(seconds: 90), isTrue, reason: 'swap wait plus polls: $waited');
      expect(waited < const Duration(minutes: 3), isTrue, reason: 'not the whole 6 min window');
    });

    test('a trial that outlives the confirm window is not called done', () async {
      final h = hashOf(image(v020));
      final dfu = FakeDfu(
        slotScript: [
          null,
          [PodSlot(slot: 0, hash: h, active: true)],
        ],
      );
      final f = FirmwareUpdateFlow(
        source: FakeSource(rel: release(v020), bytes: image(v020)),
        dfu: dfu,
        pod: FakePod(after: v020),
        wait: (_) async {},
        confirmTimeout: const Duration(seconds: 30),
      );
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.failed);
      expect(f.state.message, contains("didn't confirm"));
    });

    test('a dropped link during the upload is retried, then given up', () async {
      final once = FakeDfu(dropTimes: 1);
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), once, FakePod(after: v020));
      await f.check();
      await f.start();
      expect((f.state.phase, once.uploads), (OtaPhase.done, 2));

      final always = FakeDfu(dropTimes: 9);
      final pod = FakePod(after: v010);
      final g = flow(FakeSource(rel: release(v020), bytes: image(v020)), always, pod);
      await g.check();
      await g.start();
      expect((g.state.phase, always.uploads, pod.reclaims), (OtaPhase.failed, 3, 1));
    });

    test('an image the pod already runs, confirmed, is not uploaded again', () async {
      final h = hashOf(image(v020));
      final dfu = FakeDfu(
        slotScript: [
          [PodSlot(slot: 0, hash: h, active: true, confirmed: true)],
        ],
      );
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), dfu, FakePod(version: null, after: v020));
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.done);
      expect(dfu.uploads, 0);
    });

    test('a trial still running from an earlier update blocks a new upload', () async {
      final dfu = FakeDfu(
        slotScript: [
          [PodSlot(slot: 0, hash: Uint8List(32), active: true)],
        ],
      );
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), dfu, FakePod(after: v010));
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.failed);
      expect(f.state.message, contains('still trying out'));
      expect(dfu.uploads, 0);
    });

    test("a download that doesn't match GitHub's digest, or its own hash, never reaches the pod", () async {
      final pod = FakePod();
      final r = FirmwareRelease(
        version: v020,
        tag: 'v0.2.0',
        assetName: 'atrium-pod-0.2.0.bin',
        url: Uri.parse('https://github.com/x.bin'),
        size: 0,
        sha256: '00' * 32,
      );
      final f = flow(FakeSource(rel: r, bytes: image(v020)), FakeDfu(), pod);
      await f.check();
      await f.start();
      expect(f.state.message, contains("doesn't match the published one"));
      final g = flow(FakeSource(rel: release(v020), bytes: image(v020, corrupt: true)), FakeDfu(), pod);
      await g.check();
      await g.start();
      expect(g.state.message, contains('damaged'));
      expect(pod.releases, 0);
    });

    test('MCUmgr errors become plain words: low battery (EBUSY 10), pairing, dropped link', () {
      expect(otaErrorFor('McuMgrErrorException: rc: 10').message, contains('battery is low'));
      expect(otaErrorFor('Remote error: Busy').message, contains('battery is low'));
      expect(otaErrorFor('Insufficient encryption'), isNot(isA<OtaLinkLost>()));
      expect(otaErrorFor('Insufficient encryption').message, contains('pairing'));
      expect(otaErrorFor('Device disconnected'), isA<OtaLinkLost>());
      expect(otaErrorFor('weird').message, contains('weird'));
    });

    test('an upload error fails, gives the link back and says which version the pod is on', () async {
      final pod = FakePod(after: v010);
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), FakeDfu(fail: 'The upload failed: SMP timeout'), pod);
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.failed);
      expect(f.state.message, allOf(contains('SMP timeout'), contains('The pod is on 0.1.0')));
      expect(pod.reclaims, 1);
    });

    test('below 3.60 V off the dock the update refuses to start and nothing is downloaded', () async {
      final src = FakeSource(rel: release(v020), bytes: image(v020));
      final pod = FakePod(block: 'The battery is too low to update off the Plinth.');
      final f = flow(src, FakeDfu(), pod);
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.available);
      expect(f.state.message, contains('too low'));
      expect((src.downloads, pod.releases), (0, 0));
    });

    test('a file that is not the announced version never reaches the pod', () async {
      final pod = FakePod();
      final f = flow(FakeSource(rel: release(v020), bytes: image(const PodVersion(0, 3, 0))), FakeDfu(), pod);
      await f.check();
      await f.start();
      expect(f.state.phase, OtaPhase.failed);
      expect(f.state.message, contains('Nothing was sent'));
      expect(pod.releases, 0);
    });

    test('cancel during the upload stops it and reconnects the app', () async {
      final pod = FakePod(after: v010);
      final dfu = FakeDfu()..gate = Completer<void>();
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), dfu, pod);
      await f.check();
      final run = f.start();
      await pumpEventQueue();
      expect(f.state.phase, OtaPhase.uploading);
      expect(f.state.canCancel, isTrue);
      await f.cancel();
      await run;
      expect(f.state.phase, OtaPhase.cancelled);
      expect(f.state.message, contains('still on 0.1.0'));
      expect(pod.reclaims, 1);
    });

    test('cancel during the download stops before the link is handed over', () async {
      final src = FakeSource(rel: release(v020), bytes: image(v020))..gate = Completer<void>();
      final pod = FakePod();
      final f = flow(src, FakeDfu(), pod);
      await f.check();
      final run = f.start();
      await pumpEventQueue();
      expect(f.state.phase, OtaPhase.downloading);
      await f.cancel();
      src.gate!.complete();
      await run;
      expect(f.state.phase, OtaPhase.cancelled);
      expect(pod.releases, 0);
    });

    test('after an update, checking again finds the pod up to date', () async {
      final f = flow(FakeSource(rel: release(v020), bytes: image(v020)), FakeDfu(), FakePod(after: v020));
      await f.check();
      await f.start();
      await f.check();
      expect(f.history.last, OtaPhase.upToDate);
    });
  });
}
