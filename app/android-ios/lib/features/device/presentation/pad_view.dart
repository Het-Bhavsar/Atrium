import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/device/presentation/device_widgets.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Skin pad (#25): how many pads are left, the six change steps, and the next side.
class PadView extends ConsumerWidget {
  const PadView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final t = AtriumColors.of(context);
    final next = s.side.other.word;
    final steps = [
      ('Take the pod off', 'Tip it off from its bottom end with a fingertip, low and slow. The pad comes with it.'),
      ('Clean the base', 'Peel the old pad off the pod from one end. Wipe the base with an alcohol wipe and let it dry. Never reuse a pad.'),
      ('Charge', 'Put the pod on the Plinth with no pad on, either way round.'),
      ('Prepare your skin', 'Wipe the spot with an alcohol wipe and let it dry: no lotion, no sweat, no hair in the way.'),
      ('Pick up a new pad', 'Press the pod, window end first, into a window on the pad card. Hold for 3 seconds, then lift.'),
      (
        'Put it on',
        'Follow the fit guide on your $next temporal region, then press firmly for 10 seconds. Skip hard workouts, showers and swims for about 30 minutes.',
      ),
    ];

    return PlainPage(
      title: 'Skin pad',
      back: GoRouterState.of(context).uri.pathSegments.first == 'today'
          ? 'Today'
          : GoRouterState.of(context).uri.pathSegments.first == 'settings'
          ? 'Settings'
          : 'Device',
      children: [
        CutCard(
          image: 'pod-skin',
          semanticLabel: 'The skin side of the pod',
          caption: Column(
            children: [
              Text(
                '${s.padsLeft} ${s.padsLeft == 1 ? 'pad' : 'pads'} left on this card',
                textAlign: TextAlign.center,
                style: AtriumType.serifAt(22, color: t.ink, height: 1.15),
              ),
              const SizedBox(height: 3),
              Text(
                'Worn ${s.padHours} h · change after 48 h, at every charge, or when an edge lifts',
                textAlign: TextAlign.center,
                style: AtriumType.secondary(t.ink2),
              ),
            ],
          ),
        ),
        const SectionHeader('Change your pad'),
        ListGroup(
          children: [for (var i = 0; i < steps.length; i++) _StepRow(number: i + 1, title: steps[i].$1, text: steps[i].$2)],
        ),
        const SizedBox(height: 14),
        NoticeCard(
          icon: 'ruler',
          title: 'Next time: $next temporal',
          body: 'Switching sides at each change rests your skin. If your skin stays red or itchy, take a break.',
        ),
        const SizedBox(height: 6),
        PrimaryButton('Open the fit guide', onPressed: () => context.push('/fit')),
        const SizedBox(height: Space.m),
        GreyButton(
          "I've changed the pad",
          onPressed: () {
            final n = ref.read(appStateProvider.notifier);
            n.padChanged();
            showAtriumToast(context, 'New pad on. ${ref.read(appStateProvider).padsLeft} left on the card.');
            if (context.canPop()) context.pop();
          },
        ),
        if (s.padsLeft <= 1) const ListFoot('One pad left on this card. Time to get a new card.'),
      ],
    );
  }
}

class _StepRow extends StatelessWidget {
  const _StepRow({required this.number, required this.title, required this.text});
  final int number;
  final String title;
  final String text;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: Space.l, vertical: 14),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 22,
            child: Padding(
              padding: const EdgeInsets.only(top: 2),
              child: Text('$number', style: AtriumType.serifAt(20, color: t.ink3, height: 1)),
            ),
          ),
          const SizedBox(width: Space.m),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(title, style: AtriumType.bodyStrong(t.ink)),
                const SizedBox(height: 3),
                Text(text, style: AtriumType.secondary(t.ink2).copyWith(fontSize: 13.5, height: 1.38)),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
