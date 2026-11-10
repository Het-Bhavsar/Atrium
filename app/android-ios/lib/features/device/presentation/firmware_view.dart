// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:math' as math;

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/device/data/firmware_update_provider.dart';
import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';
import 'package:atrium/features/device/presentation/device_widgets.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Pod software update (#21). Real-data builds: the latest signed release from GitHub, sent over MCUmgr SMP
/// (`firmware_update.dart`), and "updated" only once the restarted pod reports the new version. Demo builds simulate
/// the flow and say so on screen.
class FirmwareView extends StatelessWidget {
  const FirmwareView({super.key});

  @override
  Widget build(BuildContext context) => kRealData ? const RealFirmwareView() : const _DemoFirmware();
}

/// Phase titles; [OtaState.message] carries the detail.
String otaTitle(OtaState s, {bool blocked = false}) => switch (s.phase) {
  OtaPhase.idle || OtaPhase.checking => 'Checking for pod software',
  OtaPhase.available => blocked ? 'Not ready to update yet' : 'Ready to update',
  OtaPhase.upToDate => 'Up to date',
  OtaPhase.downloading => 'Downloading',
  OtaPhase.handingOver => 'Getting the pod ready',
  OtaPhase.uploading => 'Sending to the pod',
  OtaPhase.restarting => 'Restarting the pod',
  OtaPhase.confirming => 'Trying out the new software',
  OtaPhase.verifying => 'Checking the new version',
  OtaPhase.pending => 'Installed, confirmation pending',
  OtaPhase.unconfirmed => 'Installed, not confirmed yet',
  OtaPhase.done => 'Updated',
  OtaPhase.failed => s.release == null ? "Couldn't check" : "Update didn't finish",
  OtaPhase.cancelled => 'Update stopped',
};

/// The real update screen; public so tests can drive it with fake GitHub, MCUmgr and pod providers.
class RealFirmwareView extends ConsumerStatefulWidget {
  const RealFirmwareView({super.key});

  @override
  ConsumerState<RealFirmwareView> createState() => _RealFirmwareViewState();
}

class _RealFirmwareViewState extends ConsumerState<RealFirmwareView> {
  @override
  void initState() {
    super.initState();
    Future.microtask(() {
      final s = ref.read(firmwareUpdateProvider);
      if (s.phase == OtaPhase.idle) unawaited(ref.read(firmwareUpdateProvider.notifier).check());
    });
  }

  @override
  Widget build(BuildContext context) {
    final s = ref.watch(firmwareUpdateProvider);
    final n = ref.read(firmwareUpdateProvider.notifier);
    // Rebuild on battery and connection changes, so the gate below stays current.
    ref.watch(podStatusProvider);
    final t = AtriumColors.of(context);
    final onPod = s.current ?? PodVersion.tryParse(ref.watch(podSessionProvider.select((p) => p.firmware)));
    final r = s.release;
    final blocker = s.phase == OtaPhase.available ? n.blocker : null;
    final showMeter =
        s.phase == OtaPhase.downloading ||
        s.phase == OtaPhase.uploading ||
        s.phase == OtaPhase.restarting ||
        s.phase == OtaPhase.confirming ||
        s.phase == OtaPhase.verifying ||
        s.phase == OtaPhase.pending;
    final pct = (s.progress * 100).clamp(0, 100).round();

    final detail = switch (s.phase) {
      OtaPhase.available when blocker != null => blocker,
      OtaPhase.available => 'Keep the pod near your phone until it finishes. It restarts at the end.',
      OtaPhase.upToDate => 'Your pod runs the latest published software.',
      OtaPhase.downloading => 'Getting the signed release from GitHub.',
      OtaPhase.handingOver => 'Pausing sync so the update has the Bluetooth link to itself.',
      OtaPhase.uploading => s.message ?? 'Keep the pod near your phone until this finishes.',
      OtaPhase.restarting => 'The pod restarts and moves the new software into place. This takes a few seconds.',
      OtaPhase.confirming => 'The pod keeps new software only after it has run well for a minute. Keep it close.',
      OtaPhase.verifying => 'Reconnecting to read the version the pod now runs.',
      OtaPhase.pending =>
        'The pod runs ${s.current ?? r?.version} and is still deciding whether to keep it. Atrium keeps asking it for about five minutes. Keep it close.',
      OtaPhase.done => 'Pod software ${s.current} passed the pod\'s own check and is now kept.',
      _ => s.message ?? '',
    };

    final VoidCallback? action;
    final String label;
    switch (s.phase) {
      case OtaPhase.available:
        label = r == null ? 'Update now' : 'Update to ${r.version}';
        action = blocker == null ? n.start : null;
      case OtaPhase.downloading || OtaPhase.uploading:
        label = 'Stop the update';
        action = n.cancel;
      case OtaPhase.idle || OtaPhase.checking:
        label = 'Checking…';
        action = null;
      case OtaPhase.handingOver || OtaPhase.restarting || OtaPhase.confirming || OtaPhase.verifying:
        label = 'Updating…';
        action = null;
      case OtaPhase.pending:
        label = 'Waiting for the pod…';
        action = null;
      case OtaPhase.unconfirmed:
        label = 'Check the pod again';
        action = n.recheck;
      case OtaPhase.done || OtaPhase.upToDate || OtaPhase.failed || OtaPhase.cancelled:
        label = 'Check again';
        action = n.check;
    }

    return PlainPage(
      title: 'Pod software',
      back: 'Device',
      children: [
        AtriumCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              _VersionRow(name: 'On your pod', value: onPod?.toString() ?? 'Unknown'),
              const SizedBox(height: Space.s),
              _VersionRow(name: 'Latest release', value: r?.version.toString() ?? (s.working ? '…' : 'None found')),
              if (r != null && r.notes.isNotEmpty) ...[const SizedBox(height: Space.m), Text(_trim(r.notes), style: AtriumType.secondary(t.ink))],
            ],
          ),
        ),
        const SizedBox(height: Space.m),
        AtriumCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  if (s.phase == OtaPhase.done || s.phase == OtaPhase.upToDate)
                    Padding(
                      padding: const EdgeInsets.only(right: Space.s),
                      child: AIcon('check', size: 18, color: t.green, stroke: 2.2),
                    ),
                  Expanded(
                    child: Text(otaTitle(s, blocked: blocker != null), style: AtriumType.bodyStrong(t.ink)),
                  ),
                  if (showMeter && s.phase != OtaPhase.verifying && s.phase != OtaPhase.pending) Text('$pct%', style: AtriumType.body(t.ink2)),
                ],
              ),
              if (showMeter) ...[
                const SizedBox(height: 10),
                Meter(s.phase == OtaPhase.restarting || s.phase == OtaPhase.verifying || s.phase == OtaPhase.pending ? 1 : s.progress),
              ],
              if (detail.isNotEmpty) ...[const SizedBox(height: 10), Text(detail, style: AtriumType.secondary(s.phase == OtaPhase.failed ? t.red : t.ink2))],
            ],
          ),
        ),
        const SizedBox(height: Space.xl),
        PrimaryButton(label, onPressed: action),
        const FootNote(
          'Releases come from github.com/$releaseRepo; only the release is fetched, nothing about you or your pod is sent. '
          'The software is signed, and the pod checks the signature before running it. It needs the pod on the Plinth, or above 3.60 V off it.',
        ),
      ],
    );
  }

  static String _trim(String s) => s.length <= 600 ? s : '${s.substring(0, 600).trimRight()}…';
}

class _VersionRow extends StatelessWidget {
  const _VersionRow({required this.name, required this.value});
  final String name;
  final String value;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Row(
      children: [
        Expanded(child: Text(name, style: AtriumType.body(t.ink2))),
        Text(value, style: AtriumType.bodyStrong(t.ink)),
      ],
    );
  }
}

/// Demo builds: the same steps on a timer, with nothing sent anywhere, and labelled as a demo throughout.
class _DemoFirmware extends ConsumerStatefulWidget {
  const _DemoFirmware();

  @override
  ConsumerState<_DemoFirmware> createState() => _DemoFirmwareState();
}

class _DemoFirmwareState extends ConsumerState<_DemoFirmware> {
  final _rng = math.Random();
  Timer? _timer;
  double _p = 0;

  @override
  void initState() {
    super.initState();
    // A demo left mid-way (page closed) never finished; offer it again.
    if (ref.read(appStateProvider).firmware == FirmwareState.updating) {
      Future.microtask(() => ref.read(appStateProvider.notifier).update((s) => s.copyWith(firmware: FirmwareState.available)));
    }
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  bool get _running => _timer != null;

  void _start() {
    ref.read(appStateProvider.notifier).update((s) => s.copyWith(firmware: FirmwareState.updating));
    setState(() {
      _p = 0;
      _timer = Timer(const Duration(milliseconds: 200), _tick);
    });
  }

  void _tick() {
    if (!mounted) return;
    setState(() => _p = math.min(100, _p + 4 + _rng.nextDouble() * 6));
    if (_p < 100) {
      _timer = Timer(const Duration(milliseconds: 220), _tick);
    } else {
      _timer = Timer(const Duration(milliseconds: 1200), () {
        if (!mounted) return;
        ref.read(appStateProvider.notifier).update((s) => s.copyWith(firmware: FirmwareState.done));
        setState(() => _timer = null);
        showAtriumToast(context, 'Demo finished. Nothing was sent to a pod.');
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final s = ref.watch(appStateProvider);
    final t = AtriumColors.of(context);
    final d = ref.watch(podStatusProvider);
    final ok = d.onPlinth || d.batteryPercent >= 40;

    final banner = AtriumCard(
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          AIcon('chip', size: 18, color: t.ink2),
          const SizedBox(width: Space.m),
          Expanded(
            child: Text(
              'Demo. Nothing is sent to a pod. A real build downloads the signed release and sends it over Bluetooth.',
              style: AtriumType.secondary(t.ink),
            ),
          ),
        ],
      ),
    );

    final List<Widget> body;
    if (s.firmware == FirmwareState.done) {
      body = [
        banner,
        const SizedBox(height: Space.m),
        AtriumCard(
          padding: const EdgeInsets.fromLTRB(Space.xl, 28, Space.xl, 28),
          child: Column(
            children: [
              AIcon('check', size: 44, color: t.green, stroke: 2),
              const SizedBox(height: 10),
              Text(
                'Demo finished',
                textAlign: TextAlign.center,
                style: AtriumType.serifAt(28, color: t.ink),
              ),
              const SizedBox(height: 6),
              Text(
                'In a real build the pod restarts on the new software, and Atrium reads its version before it says the update worked.',
                textAlign: TextAlign.center,
                style: AtriumType.body(t.ink2),
              ),
            ],
          ),
        ),
      ];
    } else {
      final state = !_running ? (ok ? 'Ready (demo)' : 'Needs more battery (demo)') : (_p < 100 ? 'Sending to the pod (demo)' : 'Restarting the pod (demo)');
      body = [
        banner,
        const SizedBox(height: Space.m),
        AtriumCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Newer pod software (demo)', style: AtriumType.bodyStrong(t.ink)),
              const SizedBox(height: Space.s),
              Text('Sample release notes: steadier heart rate while you walk, and faster sync after a few days away.', style: AtriumType.body(t.ink)),
              const SizedBox(height: 10),
              Text(
                'A real update is signed, and the pod goes back to its previous software by itself if the new one does not run well.',
                style: AtriumType.secondary(t.ink2),
              ),
            ],
          ),
        ),
        const SizedBox(height: Space.m),
        AtriumCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Expanded(child: Text(state, style: AtriumType.bodyStrong(t.ink))),
                  if (_running) Text('${_p.round()}%', style: AtriumType.body(t.ink2)),
                ],
              ),
              const SizedBox(height: 10),
              Meter(_p / 100),
              const SizedBox(height: 10),
              Text(
                ok ? 'Simulated progress; no pod is involved.' : 'A real pod needs the Plinth, or enough battery off it.',
                style: AtriumType.secondary(t.ink2),
              ),
            ],
          ),
        ),
        const SizedBox(height: Space.xl),
        PrimaryButton(_running ? 'Running the demo…' : 'Run the demo', onPressed: ok && !_running ? _start : null),
      ];
    }

    return PlainPage(title: 'Pod software', back: 'Device', children: body);
  }
}
