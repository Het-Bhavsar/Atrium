// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// The demo build's pod software update: the real update flow and screen ([FirmwareUpdateFlow], `RealFirmwareView`)
/// against a simulated release, upload and pod, so every step a user sees can be walked through without a pod. Nothing
/// is downloaded or sent anywhere. The simulated pod tries the image, confirms it after its health check and comes
/// back on the new version, as the firmware contract describes (docs/ble_gatt.md section 9).
library;

import 'dart:async';
import 'dart:typed_data';

import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';

const PodVersion demoFrom = PodVersion(0, 1, 0);
const PodVersion demoTo = PodVersion(0, 1, 1);

/// A small but valid MCUboot image for [v]: header, body, and the TLV area with the SHA-256 of header and body.
Uint8List demoImage(PodVersion v, {int body = 4096}) {
  const header = 32;
  const tlv = 4 + 36;
  final b = ByteData(header + body + tlv)
    ..setUint32(0, McubootImage.magic, Endian.little)
    ..setUint16(8, header, Endian.little)
    ..setUint32(12, body, Endian.little)
    ..setUint8(20, v.major)
    ..setUint8(21, v.minor)
    ..setUint16(22, v.patch, Endian.little);
  for (var i = 0; i < body; i++) {
    b.setUint8(header + i, (i * 31 + 7) & 0xFF);
  }
  final o = header + body;
  b
    ..setUint16(o, McubootImage.tlvMagic, Endian.little)
    ..setUint16(o + 2, tlv, Endian.little)
    ..setUint16(o + 4, McubootImage.tlvSha256, Endian.little)
    ..setUint16(o + 6, 32, Endian.little);
  final bytes = b.buffer.asUint8List();
  bytes.setRange(o + 8, o + 40, sha256Of(Uint8List.sublistView(bytes, 0, o)));
  return bytes;
}

/// The demo's "latest release": the version the real release carries. The screen says it is a demo.
class DemoFirmwareSource implements FirmwareSource {
  DemoFirmwareSource({this.step = const Duration(milliseconds: 120)});
  final Duration step;
  late final Uint8List _image = demoImage(demoTo);

  @override
  Future<FirmwareRelease?> latest() async {
    await Future<void>.delayed(const Duration(milliseconds: 600));
    return FirmwareRelease(
      version: demoTo,
      tag: 'demo',
      assetName: 'atrium-pod-$demoTo.bin',
      url: Uri.parse('https://github.com/$releaseRepo/releases/latest'),
      size: _image.length,
      sha256: hexOf(sha256Of(_image)),
    );
  }

  @override
  Future<Uint8List> download(FirmwareRelease r, {required void Function(int got, int total) onProgress, required bool Function() isCancelled}) async {
    for (var i = 1; i <= 10; i++) {
      await Future<void>.delayed(step);
      if (isCancelled()) throw const OtaCancelled();
      onProgress(_image.length * i ~/ 10, _image.length);
    }
    return _image;
  }
}

/// The demo's upload and slot list: progress over a few seconds, then the image on trial, then confirmed.
class DemoDfu implements DfuTransport {
  DemoDfu({this.step = const Duration(milliseconds: 150), this.trialReads = 2});
  final Duration step;

  /// Slot reads after the restart that still show the image on trial, before the pod's health check confirms it.
  final int trialReads;
  Uint8List? _hash;
  final Uint8List _old = Uint8List(32);
  int _reads = 0;
  bool _cancel = false;

  @override
  Future<void> upload(
    String deviceId,
    Uint8List image,
    Uint8List hash, {
    required void Function(int sent, int total) onProgress,
    void Function()? onReset,
  }) async {
    _cancel = false;
    for (var i = 1; i <= 25; i++) {
      await Future<void>.delayed(step);
      if (_cancel) throw const OtaCancelled();
      onProgress(image.length * i ~/ 25, image.length);
    }
    _hash = hash;
    _reads = 0;
    onReset?.call();
  }

  @override
  Future<List<PodSlot>> slots(String deviceId) async {
    final h = _hash;
    if (h == null) return [PodSlot(slot: 0, hash: _old, active: true, confirmed: true)];
    final confirmed = _reads++ >= trialReads;
    return [PodSlot(slot: 0, hash: h, active: true, confirmed: confirmed), PodSlot(slot: 1, hash: _old, confirmed: !confirmed)];
  }

  /// True once an upload has finished and the pod restarted on it.
  bool get installed => _hash != null;

  @override
  Future<void> cancel() async => _cancel = true;
}

/// The demo pod: on 0.1.0 until an upload has finished, then on 0.1.1. It is always reachable and charged enough.
class DemoPodHandover implements PodHandover {
  DemoPodHandover(this.dfu);
  final DemoDfu dfu;

  PodVersion get version => dfu.installed ? demoTo : demoFrom;

  @override
  Future<PodVersion?> readVersion() async => version;

  @override
  String? blocker() => null;

  @override
  Future<String> release() async => 'demo-pod';

  @override
  Future<PodVersion?> reclaim(Duration timeout) async {
    await Future<void>.delayed(const Duration(milliseconds: 800));
    return version;
  }
}
