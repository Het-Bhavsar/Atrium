import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 7 (#24): the glasses question decides where the pod sits.
class FitView extends ConsumerWidget {
  const FitView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    void choose(bool glasses) {
      ref.read(appStateProvider.notifier).update((s) => s.copyWith(glasses: glasses));
      context.push('/place');
    }

    return OnboardingScaffold(
      back: context.canPop(),
      content: [
        const StepsDots(6),
        const ObTitle('Do you wear glasses?'),
        const ObLede('It decides where the pod sits. You can change this later in Settings.'),
        // Both cards take the taller one's height, as in the prototype's grid.
        IntrinsicHeight(
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Expanded(
                child: _Choice(image: 'fit-noglasses', title: 'No', note: 'The temporal hollow', onTap: () => choose(false)),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: _Choice(image: 'fit-glasses', title: 'Yes', note: 'Just above the glasses arm', onTap: () => choose(true)),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _Choice extends StatelessWidget {
  const _Choice({required this.image, required this.title, required this.note, required this.onTap});
  final String image;
  final String title;
  final String note;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return AtriumCard(
      onTap: onTap,
      padding: const EdgeInsets.fromLTRB(10, 10, 10, 14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          AspectRatio(
            aspectRatio: 4 / 5,
            child: ClipRRect(
              borderRadius: BorderRadius.circular(14),
              child: Image.asset('assets/img/$image.jpg', fit: BoxFit.cover, alignment: const Alignment(0, -0.4)),
            ),
          ),
          const SizedBox(height: 12),
          Text(title, style: AtriumType.serifAt(24, color: t.ink, height: 1)),
          const SizedBox(height: 4),
          Text(note, style: AtriumType.secondary(t.ink2)),
        ],
      ),
    );
  }
}
