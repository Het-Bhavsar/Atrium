// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/features/device/data/github_release_source.dart';
import 'package:atrium/features/device/data/mcumgr_transport.dart';
import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Where pod software comes from, and how it reaches the pod (overridden in tests).
final firmwareSourceProvider = Provider<FirmwareSource>((ref) => GitHubReleaseSource());
final dfuTransportProvider = Provider<DfuTransport>((ref) => McumgrTransport());

/// Real-data builds: the pod software update (#21). It lives above the screen, so leaving Pod software doesn't stop it.
final firmwareUpdateProvider = NotifierProvider<FirmwareUpdateNotifier, OtaState>(FirmwareUpdateNotifier.new);

class FirmwareUpdateNotifier extends Notifier<OtaState> {
  late FirmwareUpdateFlow _flow;

  @override
  OtaState build() {
    _flow = FirmwareUpdateFlow(
      source: ref.read(firmwareSourceProvider),
      dfu: ref.read(dfuTransportProvider),
      pod: _SessionHandover(ref),
      onChange: (s) => state = s,
    );
    return _flow.state;
  }

  Future<void> check() => _flow.check();
  Future<void> start() => _flow.start();
  Future<void> cancel() => _flow.cancel();

  /// Why [start] would refuse now, for the button (battery and connection).
  String? get blocker => _SessionHandover(ref).blocker();
}

/// The app's pod link (PodSession) stepping aside for MCUmgr and coming back after the restart.
class _SessionHandover implements PodHandover {
  _SessionHandover(this.ref);
  final Ref ref;

  @override
  Future<PodVersion?> readVersion() async {
    final link = ref.read(podLinkProvider);
    final session = ref.read(podSessionProvider);
    if (link != null && session.link == LinkState.connected && !session.handedOver) {
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
    final pod = ref.read(appStateProvider).pod;
    return updateBlocker(connected: pod.connected, docked: pod.onPlinth, vsysMv: pod.vsysMv, batteryLow: pod.batteryLow);
  }

  @override
  Future<String> release() => ref.read(podSessionProvider.notifier).handOver();

  @override
  Future<PodVersion?> reclaim(Duration timeout) async => PodVersion.tryParse(await ref.read(podSessionProvider.notifier).takeBack(timeout));
}
