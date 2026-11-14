// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';

import 'package:atrium/core/config.dart';
import 'package:atrium/features/device/data/demo_firmware.dart';
import 'package:atrium/features/device/data/github_release_source.dart';
import 'package:atrium/features/device/data/mcumgr_transport.dart';
import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Where pod software comes from, and how it reaches the pod (overridden in tests).
/// The demo build runs the same flow and screen against a simulated release, upload and pod (`demo_firmware.dart`).
final firmwareSourceProvider = Provider<FirmwareSource>((ref) => kRealData ? GitHubReleaseSource() : DemoFirmwareSource());
final dfuTransportProvider = Provider<DfuTransport>((ref) => kRealData ? McumgrTransport() : DemoDfu());

/// The pod software update (#21). It lives above the screen, so leaving Pod software doesn't stop it.
final firmwareUpdateProvider = NotifierProvider<FirmwareUpdateNotifier, OtaState>(FirmwareUpdateNotifier.new);

class FirmwareUpdateNotifier extends Notifier<OtaState> {
  late FirmwareUpdateFlow _flow;

  late PodHandover _pod;

  @override
  OtaState build() {
    final dfu = ref.read(dfuTransportProvider);
    _pod = kRealData || dfu is! DemoDfu ? _SessionHandover(ref) : DemoPodHandover(dfu);
    _flow = FirmwareUpdateFlow(
      source: ref.read(firmwareSourceProvider),
      dfu: dfu,
      pod: _pod,
      onChange: (s) => state = s,
      // The demo pod swaps and confirms in seconds; the real waits are for the pod's own timing.
      swapWait: kRealData ? const Duration(seconds: 10) : const Duration(seconds: 1),
      confirmPoll: kRealData ? const Duration(seconds: 10) : const Duration(milliseconds: 1500),
    );
    return _flow.state;
  }

  Future<void> check() => _flow.check();
  Future<void> start() => _flow.start();
  Future<void> recheck() => _flow.recheck();
  Future<void> cancel() => _flow.cancel();

  /// Why [start] would refuse now, for the button (battery and connection).
  String? get blocker => _pod.blocker();
}

/// The app's pod link (PodSession) stepping aside for MCUmgr and coming back after the restart.
class _SessionHandover implements PodHandover {
  _SessionHandover(this.ref);
  final Ref ref;

  @override
  Future<PodVersion?> readVersion() async {
    final link = ref.read(podLinkProvider);
    final session = ref.read(podSessionProvider);
    if (link != null && session.connected) {
      try {
        return PodVersion.tryParse(await link.firmwareRevision());
      } on Object {
        // Fall back to what was read at connect.
      }
    }
    return PodVersion.tryParse(session.firmware);
  }

  @override
  String? blocker() {
    final pod = ref.read(podStatusProvider);
    return updateBlocker(connected: pod.connected, docked: pod.onPlinth, vsysMv: pod.vsysMv, batteryLow: pod.batteryLow);
  }

  @override
  Future<String> release() => ref.read(podSessionProvider.notifier).handOver();

  @override
  Future<PodVersion?> reclaim(Duration timeout) async => PodVersion.tryParse(await ref.read(podSessionProvider.notifier).takeBack(timeout));
}
