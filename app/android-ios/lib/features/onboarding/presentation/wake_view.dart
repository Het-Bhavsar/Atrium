import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 4 (#15, #17): lifting the pod off the Plinth wakes it.
class WakeView extends StatelessWidget {
  const WakeView({super.key});

  @override
  Widget build(BuildContext context) => OnboardingScaffold(
    topInset: 0,
    content: const [
      ObCutout('assets/img/pod-plinth.png', widthFactor: 0.78, maxWidth: 300, height: 270),
      StepsDots(3),
      ObTitle('Lift the pod off the Plinth'),
      ObLede('Lifting it off wakes it. It looks for your phone for one minute.'),
      Spacer(),
    ],
    footer: [
      PrimaryButton("It's off", onPressed: () => context.push('/scan')),
      PlainButton(
        "The pod doesn't wake",
        onPressed: () => showObSheet(
          context,
          title: 'Charge it first',
          paragraphs: const ['A new pod ships switched off. Plug the Plinth into USB-C and set the pod on it, either way round. It wakes on the Plinth.'],
        ),
      ),
    ],
  );
}
