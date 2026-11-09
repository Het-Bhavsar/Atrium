// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';

import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 6 (#15, #21): pairing checklist, then the pod software check.
class PairView extends ConsumerStatefulWidget {
  const PairView({super.key});

  @override
  ConsumerState<PairView> createState() => _PairViewState();
}

class _PairViewState extends ConsumerState<PairView> with SingleTickerProviderStateMixin {
  String _firmware = '0.1.0';
  String? _error;
  static const _steps = ['Connecting', 'Securing the link', "Setting the pod's clock", 'Checking pod software'];

  late final AnimationController _spin = AnimationController(vsync: this, duration: const Duration(milliseconds: 800))..repeat();
  final _timers = <Timer>[];

  /// Index of the step in progress; steps below it are done. -1 before start, [_steps.length] when finished.
  int _now = -1;

  bool get _done => _now >= _steps.length;

  @override
  void initState() {
    super.initState();
    if (kRealData) {
      WidgetsBinding.instance.addPostFrameCallback((_) => _pairForReal());
      return;
    }
    // Demo: a step every 0.9 s from 0.3 s, as in the prototype.
    for (var i = 0; i <= _steps.length; i++) {
      _timers.add(
        Timer(Duration(milliseconds: 300 + i * 900), () {
          if (mounted) setState(() => _now = i);
        }),
      );
    }
  }

  /// Real pairing: connect and bond (steps 1-2), the clock is set on connect (3), then read the firmware (4).
  Future<void> _pairForReal() async {
    final pod = GoRouterState.of(context).extra as FoundPod?;
    final link = ref.read(podLinkProvider);
    if (pod == null || link == null) {
      setState(() => _error = 'No pod selected. Go back and find it again.');
      return;
    }
    try {
      setState(() => _now = 0);
      await link.connect(pod.id);
      setState(() => _now = 3);
      _firmware = (await link.firmwareRevision()).split('+').first;
      await ref.read(podSessionProvider.notifier).afterConnect(name: pod.name);
      if (mounted) setState(() => _now = _steps.length);
    } on Object catch (e) {
      if (mounted) setState(() => _error = podErrorText(e));
    }
  }

  @override
  void dispose() {
    for (final t in _timers) {
      t.cancel();
    }
    _spin.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return OnboardingScaffold(
      back: false,
      content: [
        const StepsDots(5),
        const ObTitle('Pairing'),
        const ObLede('This takes a few seconds. Keep the pod near your phone.'),
        ObArt(
          alignment: Alignment.centerLeft,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              for (var i = 0; i < _steps.length; i++) ...[
                if (i > 0) const SizedBox(height: 14),
                _CheckRow(label: _steps[i], done: i < _now, now: i == _now, spin: _spin),
              ],
            ],
          ),
        ),
        ConstrainedBox(
          constraints: const BoxConstraints(minHeight: 40),
          child: Text(_error ?? (_done ? 'Pod software $_firmware is up to date.' : ''), style: AtriumType.secondary(_error != null ? t.red : t.ink2)),
        ),
        const SizedBox(height: 12),
      ],
      footer: [PrimaryButton('Continue', onPressed: _done ? () => context.push('/fit') : null)],
    );
  }
}

class _CheckRow extends StatelessWidget {
  const _CheckRow({required this.label, required this.done, required this.now, required this.spin});
  final String label;
  final bool done;
  final bool now;
  final Animation<double> spin;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    const d = Duration(milliseconds: 300);
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        SizedBox(
          width: 24,
          height: 24,
          child: now
              ? RotationTransition(
                  turns: spin,
                  child: CircularProgressIndicator(value: 0.75, strokeWidth: 1.5, color: t.accent, backgroundColor: Colors.transparent),
                )
              : AnimatedContainer(
                  duration: d,
                  alignment: Alignment.center,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: done ? t.green : Colors.transparent,
                    border: Border.all(color: done ? t.green : t.hair, width: 1.5),
                  ),
                  child: AnimatedOpacity(
                    opacity: done ? 1 : 0,
                    duration: d,
                    child: const AIcon('check', size: 14, color: AtriumColors.onPhoto, stroke: 3),
                  ),
                ),
        ),
        const SizedBox(width: 12),
        AnimatedDefaultTextStyle(
          duration: d,
          style: TextStyle(fontSize: 16, color: done || now ? t.ink : t.ink3),
          child: Text(label),
        ),
      ],
    );
  }
}
