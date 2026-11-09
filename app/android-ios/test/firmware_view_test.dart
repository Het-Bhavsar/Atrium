// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:convert';
import 'dart:typed_data';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/features/device/data/firmware_update_provider.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';
import 'package:atrium/features/device/presentation/firmware_view.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:go_router/go_router.dart';

import 'firmware_update_test.dart' show FakeDfu, FakeSource, image, release, v020;

/// The real Pod software screen, end to end through the app's own pod session: GitHub and MCUmgr are fakes, and so is
/// the native host (Pigeon PodHostApi), which plays a pod on 0.1.0 that reboots onto whatever the upload carried.
class _Host extends PodHostApi {
  _Host({required this.after});

  String firmware = '0.1.0+41f1d65';

  /// Version the pod boots into after the update.
  final String after;
  final calls = <String>[];
  bool restarted = false;

  /// Link-state events go back through the app's PodLink, as native sends them over Pigeon.
  late PodLink link;

  @override
  Future<void> connect(String id) async {
    calls.add('connect $id');
    if (restarted) firmware = after;
    link.onLinkState(LinkState.connected);
  }

  @override
  Future<void> disconnect() async {
    calls.add('disconnect');
    link.onLinkState(LinkState.disconnected);
  }

  @override
  Future<String?> bondedPodId() async => 'pod-1';

  @override
  Future<Uint8List> read(int characteristic) async => switch (characteristic) {
    Gatt.firmwareRevision => Uint8List.fromList(utf8.encode(firmware)),
    Gatt.time => (ByteData(4)..setUint32(0, 1762765200, Endian.little)).buffer.asUint8List(),
    Gatt.chargeState => Uint8List(19)..[4] = 0x01, // docked
    _ => Uint8List(18),
  };

  @override
  Future<void> write(int characteristic, Uint8List value, bool withResponse) async {}

  @override
  Future<void> setNotify(int characteristic, bool enabled) async {}

  @override
  Future<void> setBackgroundSync(bool enabled, int maxHours) async {}

  @override
  Future<void> syncLog(int fromPage) async {}
}

class _Dfu extends FakeDfu {
  _Dfu(this.host);
  final _Host host;

  @override
  Future<void> upload(String deviceId, Uint8List image, {required void Function(int sent, int total) onProgress, void Function()? onReset}) async {
    expect(host.calls.last, 'disconnect', reason: 'the app lets go of the link before MCUmgr connects');
    await super.upload(deviceId, image, onProgress: onProgress, onReset: onReset);
    host.restarted = true;
  }
}

/// The log sync itself is covered in live_sync_test; here it returns at once (no file store in a widget test).
class _NoSync extends MetricsNotifier {
  @override
  Future<int> sync() async => 0;
}

Future<ProviderContainer> _open(WidgetTester tester, _Host host) async {
  final link = PodLink(host: host)..onLinkState(LinkState.connected);
  host.link = link;
  final container = ProviderContainer(
    overrides: [
      podLinkProvider.overrideWithValue(link),
      metricsProvider.overrideWith(_NoSync.new),
      firmwareSourceProvider.overrideWithValue(FakeSource(rel: release(v020), bytes: image(v020))),
      dfuTransportProvider.overrideWithValue(_Dfu(host)),
    ],
  );
  addTearDown(container.dispose);
  // On the Plinth, so the 3.60 V rule passes; the session reads the firmware revision as it does after a connect.
  container.read(appStateProvider.notifier).update((s) => s.copyWith(situation: PodSituation.charging));
  await container.read(podSessionProvider.notifier).afterConnect();
  final router = GoRouter(
    routes: [GoRoute(path: '/', builder: (_, _) => const RealFirmwareView())],
  );
  await tester.pumpWidget(
    UncontrolledProviderScope(
      container: container,
      child: MaterialApp.router(theme: buildTheme(Brightness.light), routerConfig: router),
    ),
  );
  for (var i = 0; i < 5; i++) {
    await tester.pump(const Duration(milliseconds: 100));
  }
  return container;
}

Future<void> _runUpdate(WidgetTester tester) async {
  await tester.tap(find.text('Update to 0.2.0'));
  // Download and upload, then the swap wait (10 s) before reconnecting.
  for (var i = 0; i < 30; i++) {
    await tester.pump(const Duration(milliseconds: 500));
  }
}

void main() {
  testWidgets('shows both versions, updates and says so only after the pod reports the new one', (tester) async {
    final host = _Host(after: '0.2.0+9c1e2aa');
    final c = await _open(tester, host);
    expect(find.text('0.1.0'), findsOneWidget, reason: 'on the pod, from DIS 0x2A26');
    expect(find.text('0.2.0'), findsOneWidget, reason: 'latest release');
    expect(find.text('Ready to update'), findsOneWidget);
    await _runUpdate(tester);
    expect(c.read(firmwareUpdateProvider).phase, OtaPhase.done);
    expect(find.text('Updated'), findsOneWidget);
    expect(find.textContaining('Pod software 0.2.0 is running'), findsOneWidget);
    expect(host.calls.where((x) => x.startsWith('connect')).length, 1, reason: 'reconnected once after the restart');
    expect(c.read(podSessionProvider).handedOver, isFalse, reason: 'syncing resumes');
  });

  testWidgets('a pod that boots the old image again is shown as not updated', (tester) async {
    final host = _Host(after: '0.1.0+41f1d65');
    final c = await _open(tester, host);
    await _runUpdate(tester);
    expect(c.read(firmwareUpdateProvider).phase, OtaPhase.failed);
    expect(find.text("Update didn't finish"), findsOneWidget);
    expect(find.text('Updated'), findsNothing);
  });

  testWidgets('off the Plinth with no voltage reading, the button stays off and says why', (tester) async {
    final host = _Host(after: '0.2.0');
    final c = await _open(tester, host);
    c.read(appStateProvider.notifier).update((s) => s.copyWith(situation: PodSituation.worn, livePod: PodStatus.demo[PodSituation.worn]));
    await tester.pump();
    expect(find.text('Not ready to update yet'), findsOneWidget);
    expect(find.textContaining('Put the pod on the Plinth'), findsOneWidget);
    await tester.tap(find.text('Update to 0.2.0'));
    await tester.pump();
    expect(c.read(firmwareUpdateProvider).phase, OtaPhase.available);
    expect(host.calls, isNot(contains('disconnect')), reason: 'the link is never handed over');
  });
}
