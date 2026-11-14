// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 2 (#23): what the pod reads, and the wellness notice. Each reading is marked by the colour of the light
/// that measures it (green for the pulse, infrared violet for sleep, red for skin heat), as on the Home screen; the
/// notice is plain text, not a card.
class AboutView extends StatelessWidget {
  const AboutView({super.key});

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return OnboardingScaffold(
      topInset: 0,
      footerGap: 24,
      content: [
        const ObCutout('assets/img/pod.png', widthFactor: 0.62, maxWidth: 240, height: 220),
        const StepsDots(0),
        const ObTitle('What Atrium reads'),
        const ObLede('Atrium shines light into the skin at your temporal region and measures what comes back.'),
        const SizedBox(height: 6),
        _Reading(color: t.green, title: 'Heart rate and HRV', body: 'Green light from one small optical module.'),
        _Reading(color: t.ir, title: 'Sleep', body: 'Pulse and head movement overnight.'),
        _Reading(color: t.red, title: 'Skin temperature', body: 'A sensor by the optical window reads skin heat; a second one corrects for the room.'),
        _Reading(color: t.ink3, title: 'Movement', body: 'Active minutes and how hard your day was.'),
        const SizedBox(height: 18),
        Text(
          "Atrium is for wellness and research. It doesn't diagnose, treat or prevent any condition. Don't use its readings for medical decisions.",
          style: AtriumType.secondary(t.ink2),
        ),
      ],
      footer: [PrimaryButton('I understand', onPressed: () => context.push('/you'))],
    );
  }
}

/// One reading: a short bar in its light's colour, a serif name and one plain line.
class _Reading extends StatelessWidget {
  const _Reading({required this.color, required this.title, required this.body});
  final Color color;
  final String title;
  final String body;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 9),
      child: IntrinsicHeight(
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Container(
              width: 3,
              margin: const EdgeInsets.only(top: 4, bottom: 3),
              decoration: BoxDecoration(color: color, borderRadius: BorderRadius.circular(2)),
            ),
            const SizedBox(width: 14),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(title, style: AtriumType.serifAt(21, color: t.ink, height: 1.2)),
                  const SizedBox(height: 2),
                  Text(body, style: AtriumType.secondary(t.ink2)),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
