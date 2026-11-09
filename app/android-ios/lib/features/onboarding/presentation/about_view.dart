import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 2 (#23): what the pod reads, and the wellness notice.
class AboutView extends StatelessWidget {
  const AboutView({super.key});

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    Widget tile(String icon, Color fg, Color bg) => Container(
      width: 36,
      height: 36,
      alignment: Alignment.center,
      decoration: BoxDecoration(color: bg, borderRadius: BorderRadius.circular(10)),
      child: AIcon(icon, size: 20, color: fg),
    );
    return OnboardingScaffold(
      topInset: 0,
      footerGap: 24,
      content: [
        const ObCutout('assets/img/pod.png', widthFactor: 0.62, maxWidth: 240, height: 220),
        const StepsDots(0),
        const ObTitle('What Atrium reads'),
        const ObLede('Atrium shines light into the skin at your temporal region and measures what comes back.'),
        ObFeature(
          first: true,
          leading: tile('heart', t.green, t.green.withValues(alpha: 0.16)),
          title: 'Heart rate and HRV',
          body: 'Green light from one small optical module.',
        ),
        ObFeature(leading: tile('moon', t.ir, t.ir.withValues(alpha: 0.16)), title: 'Sleep', body: 'Pulse and head movement overnight.'),
        ObFeature(
          leading: tile('temp', t.red, t.red.withValues(alpha: 0.14)),
          title: 'Skin temperature',
          body: 'A sensor by the optical window reads skin heat; a second one corrects for the room.',
        ),
        ObFeature(last: true, leading: tile('effort', t.ink2, t.card), title: 'Movement', body: 'Active minutes and how hard your day was.'),
        const SizedBox(height: 18),
        AtriumCard(
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              AIcon('info', color: t.ink2),
              const SizedBox(width: 12),
              Expanded(
                child: Text(
                  "Atrium is for wellness and research. It doesn't diagnose, treat or prevent any condition. Don't use its readings for medical decisions.",
                  style: AtriumType.secondary(t.ink2),
                ),
              ),
            ],
          ),
        ),
      ],
      footer: [PrimaryButton('I understand', onPressed: () => context.push('/you'))],
    );
  }
}
