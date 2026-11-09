import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 1 (#23): the welcome photo, the mark and one line on what Atrium measures.
class WelcomeView extends StatelessWidget {
  const WelcomeView({super.key});

  @override
  Widget build(BuildContext context) => OnPhotoScaffold(
    image: 'assets/img/welcome.jpg',
    alignment: const Alignment(0, -0.4),
    content: [
      const Spacer(),
      const Align(
        alignment: Alignment.centerLeft,
        child: Padding(padding: EdgeInsets.only(bottom: 18), child: AtriumMark()),
      ),
      const ObTitle('Atrium', size: 56, color: AtriumColors.onPhoto, bottom: 6),
      Padding(
        padding: const EdgeInsets.only(bottom: 14),
        child: Text('BY ${kMaker.toUpperCase()}', style: AtriumType.label(AtriumColors.onPhoto.withValues(alpha: 0.7))),
      ),
      ObLede('Your pulse, sleep and skin temperature, measured with light.', color: AtriumColors.onPhoto.withValues(alpha: 0.75)),
    ],
    footer: [
      PrimaryButton('Get started', onPhoto: true, onPressed: () => context.push('/about')),
      Padding(
        padding: const EdgeInsets.only(top: 6),
        child: Text(
          'Your readings stay on this phone unless you export or share them.',
          textAlign: TextAlign.center,
          style: AtriumType.caption(AtriumColors.onPhoto.withValues(alpha: 0.55)),
        ),
      ),
    ],
  );
}
