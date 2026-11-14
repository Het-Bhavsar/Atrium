// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/features/device/data/demo_firmware.dart';
import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';
import 'package:flutter_test/flutter_test.dart';

/// The demo build runs the real update flow against a simulated release, upload and pod: it must pass the same image
/// checks and walk every step a user would see, including the pod's own confirmation, and a stopped update must leave
/// the pod where it was.
void main() {
  FirmwareUpdateFlow flow(DemoDfu dfu) => FirmwareUpdateFlow(
    source: DemoFirmwareSource(step: Duration.zero),
    dfu: dfu,
    pod: DemoPodHandover(dfu),
    wait: (_) async {},
  );

  test('the demo image is a valid MCUboot image for 0.1.1', () {
    final img = McubootImage.parse(demoImage(demoTo));
    expect(img.version, demoTo);
  });

  test('the demo update walks every real step and ends confirmed on 0.1.1', () async {
    final dfu = DemoDfu(step: Duration.zero);
    final f = flow(dfu);
    await f.check();
    expect((f.state.phase, f.state.current), (OtaPhase.available, demoFrom));
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
    expect(f.state.current, demoTo);
    await f.check();
    expect(f.state.phase, OtaPhase.upToDate, reason: 'checking again finds it up to date');
  });

  test('a demo update stopped during the upload leaves the pod on 0.1.0', () async {
    final dfu = DemoDfu(step: const Duration(milliseconds: 5));
    final f = flow(dfu);
    await f.check();
    final run = f.start();
    while (f.state.phase != OtaPhase.uploading) {
      await Future<void>.delayed(const Duration(milliseconds: 2));
    }
    await f.cancel();
    await run;
    expect(f.state.phase, OtaPhase.cancelled);
    expect(f.state.current, demoFrom);
  });
}
