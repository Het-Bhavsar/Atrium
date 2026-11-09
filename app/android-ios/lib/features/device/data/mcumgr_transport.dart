// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:typed_data';

import 'package:atrium/features/device/domain/firmware_update.dart';
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
  Future<void> upload(String deviceId, Uint8List image, {required void Function(int sent, int total) onProgress, void Function()? onReset}) async {
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
        error ??= 'The pod stopped answering during the upload.';
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
    if (error != null) throw OtaFailure('The upload failed: $error');
    if (!success) throw const OtaFailure('The upload stopped before it finished.');
  }

  @override
  Future<void> cancel() async {
    _cancelled = true;
    await _manager?.cancel();
  }
}
