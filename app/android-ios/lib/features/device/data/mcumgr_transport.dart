// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:typed_data';

import 'package:atrium/features/device/domain/firmware_update.dart';
import 'package:flutter/foundation.dart' show visibleForTesting;
import 'package:mcumgr_flutter/mcumgr_flutter.dart' as mcu;
import 'package:mcumgr_flutter/models/firmware_upgrade_mode.dart';

/// MCUmgr SMP over BLE through Nordic's `mcumgr_flutter` (iOS and Android Device Manager libraries). It opens its own
/// BLE connection to the pod by platform id (iOS peripheral UUID, Android address), so the app's link must be released
/// first ([PodHandover.release]).
///
/// Mode "test": upload to the second slot, mark it for test, reset. The pod confirms the new image itself after 60 s of
/// healthy running (`firmware/src/dfu_health.c`); an image that never confirms reverts on the next reset.
/// `eraseAppSettings` stays off: the settings partition holds the pod's bonds and its mode and sleep window.
class McumgrTransport implements DfuTransport {
  mcu.FirmwareUpdateManager? _manager;
  bool _cancelled = false;

  /// A stalled upload (pod out of range with the link still up) gives up after this long without progress.
  static const stallTimeout = Duration(seconds: 60);

  @override
  Future<void> upload(String deviceId, Uint8List image, Uint8List hash, {required void Function(int sent, int total) onProgress, void Function()? onReset}) async {
    _cancelled = false;
    final m = await mcu.FirmwareUpdateManagerFactory().getUpdateManager(deviceId);
    _manager = m;
    final finished = Completer<void>();
    var success = false;
    String? error;
    Timer? stall;
    void arm() {
      stall?.cancel();
      stall = Timer(stallTimeout, () {
        error ??= 'timed out: the pod stopped answering during the upload';
        if (!finished.isCompleted) finished.complete();
        unawaited(m.cancel());
      });
    }

    final states = m.setup().listen(
      (s) {
        arm();
        if (s == mcu.FirmwareUpgradeState.reset) onReset?.call();
        if (s == mcu.FirmwareUpgradeState.success) success = true;
      },
      onError: (Object e) {
        error ??= '$e';
        if (!finished.isCompleted) finished.complete();
      },
      onDone: () {
        if (!finished.isCompleted) finished.complete();
      },
    );
    final progress = m.progressStream.listen((p) {
      arm();
      onProgress(p.bytesSent, p.imageSize);
    });
    try {
      arm();
      await m.updateWithImageData(
        imageData: image,
        hash: hash,
        configuration: const mcu.FirmwareUpgradeConfiguration(firmwareUpgradeMode: FirmwareUpgradeMode.testOnly, eraseAppSettings: false),
      );
      await finished.future;
    } finally {
      stall?.cancel();
      await states.cancel();
      await progress.cancel();
      _manager = null;
      try {
        await m.kill();
      } on Object {
        // Already gone.
      }
    }
    if (_cancelled) throw const OtaCancelled();
    if (error != null) throw otaErrorFor(error!);
    if (!success) throw const OtaLinkLost('The upload stopped before it finished.');
  }

  @override
  Future<List<PodSlot>> slots(String deviceId) async {
    final m = await mcu.FirmwareUpdateManagerFactory().getUpdateManager(deviceId);
    try {
      final list = await m.readImageList().timeout(const Duration(seconds: 20));
      return [
        for (final s in list ?? const <mcu.ImageSlot>[])
          PodSlot(slot: s.slot, hash: s.hash, active: s.active, confirmed: s.confirmed, pending: s.pending, version: s.version),
      ];
    } finally {
      try {
        await m.kill();
      } on Object {
        // Already gone.
      }
    }
  }

  @override
  Future<void> cancel() async {
    _cancelled = true;
    await _manager?.cancel();
  }
}

/// Plain words for what MCUmgr and the BLE stack report. The pod's upload hook answers MGMT_ERR_EBUSY (10) below
/// 3.60 V off the Plinth (`firmware/src/main.c`, `dfu_guard`); the SMP characteristic needs an encrypted, bonded link.
@visibleForTesting
OtaFailure otaErrorFor(String raw) {
  final t = raw.toLowerCase();
  if (t.contains('busy') || RegExp(r'\b(rc|error|code)[:= ]*10\b').hasMatch(t) || t.contains('(10)')) {
    return const OtaFailure('The pod stopped the update because its battery is low. Put it on the Plinth, then try again.');
  }
  if (t.contains('encrypt') || t.contains('authenticat') || t.contains('bond') || t.contains('pairing')) {
    return const OtaFailure("The pod didn't accept this phone's pairing for the update. Open Device, let it reconnect, then try again.");
  }
  if (t.contains('disconnect') || t.contains('connection') || t.contains('timeout') || t.contains('timed out') || t.contains('not connected')) {
    return const OtaLinkLost('The link to the pod dropped during the upload.');
  }
  return OtaFailure('The upload failed: $raw');
}
