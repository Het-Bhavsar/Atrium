// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:math' as math;

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/device/presentation/device_widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Pod software update (#21): release notes, then ready, progress and done.
/// Progress is simulated until the DFU transport lands.
class FirmwareView extends ConsumerStatefulWidget {
  const FirmwareView({super.key});

  @override
  ConsumerState<FirmwareView> createState() => _FirmwareViewState();
}

class _FirmwareViewState extends ConsumerState<FirmwareView> {
  final _rng = math.Random();
  Timer? _timer;
  double _p = 0;

  @override
  void initState() {
    super.initState();
    // An update left mid-way (page closed) never finished; offer it again.
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
        showAtriumToast(context, 'Pod software updated to 0.2.0');
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final s = ref.watch(appStateProvider);
    final t = AtriumColors.of(context);
    final d = s.pod;
    final ok = d.onPlinth || d.batteryPercent >= 40;

    final List<Widget> body;
    if (s.firmware == FirmwareState.done) {
      body = [
        AtriumCard(
          padding: const EdgeInsets.fromLTRB(Space.xl, 28, Space.xl, 28),
          child: Column(
            children: [
              AIcon('check', size: 44, color: t.green, stroke: 2),
              const SizedBox(height: 10),
              Text(
                'Up to date',
                textAlign: TextAlign.center,
                style: AtriumType.serifAt(28, color: t.ink),
              ),
              const SizedBox(height: 6),
              Text('Pod software 0.2.0 is running.', textAlign: TextAlign.center, style: AtriumType.body(t.ink2)),
            ],
          ),
        ),
      ];
    } else {
      final state = !_running ? (ok ? 'Ready to update' : 'Needs more battery') : (_p < 100 ? 'Sending to the pod' : 'Restarting the pod');
      body = [
        AtriumCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Pod software 0.2.0', style: AtriumType.bodyStrong(t.ink)),
              const SizedBox(height: Space.s),
              Text('Steadier heart rate while you walk, and faster sync after a few days away.', style: AtriumType.body(t.ink)),
              const SizedBox(height: 10),
              Text('About 2 minutes. The update is signed. If anything goes wrong, the pod goes back to 0.1.0 by itself.', style: AtriumType.secondary(t.ink2)),
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
                ok ? 'Keep the pod near your phone until it finishes.' : 'Charge the pod to 40% or put it on the Plinth first.',
                style: AtriumType.secondary(t.ink2),
              ),
            ],
          ),
        ),
        const SizedBox(height: Space.xl),
        PrimaryButton(_running ? 'Updating…' : 'Update now', onPressed: ok && !_running ? _start : null),
      ];
    }

    return PlainPage(title: 'Pod software', back: 'Device', children: body);
  }
}
