// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:math' as math;
import 'dart:ui';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Onboarding (#24, #25, #26): where the pod goes. In the Home language: the photo is the instruction, with
/// the pod ringed on it; one serif sentence, the pad step in a line, one side switch.
class PlaceView extends ConsumerWidget {
  const PlaceView({super.key});

  /// Pod centre in each photo, as a fraction of the 896 x 1120 image.
  static const _glasses = (image: 'fit-glasses', pod: Offset(0.626, 0.297), title: 'Just above the arm', note: 'With glasses');
  static const _hollow = (image: 'fit-noglasses', pod: Offset(0.643, 0.367), title: 'In the temporal hollow', note: 'No glasses');

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final t = AtriumColors.of(context);
    final g = s.glasses ?? false;
    final p = g ? _glasses : _hollow;
    final top = MediaQuery.paddingOf(context).top;

    return OnboardingScaffold(
      footerGap: 20,
      header: SizedBox(
        height: top + 452,
        child: LayoutBuilder(
          builder: (context, box) {
            final size = Size(box.maxWidth, box.maxHeight);
            const align = Alignment(0, -0.35);
            final pod = _coverPoint(size, const Size(896, 1120), align, p.pod);
            return Stack(
              fit: StackFit.expand,
              children: [
                Image.asset('assets/img/${p.image}.jpg', fit: BoxFit.cover, alignment: align),
                const DecoratedBox(
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      stops: [0, 0.18, 0.62, 1],
                      colors: [Color(0x55000000), Color(0x00000000), Color(0x00000000), Color(0xB0000000)],
                    ),
                  ),
                ),
                _PodMarker(at: pod),
                Positioned(
                  left: Space.gutter + 4,
                  right: Space.gutter,
                  bottom: 26,
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(p.note.toUpperCase(), style: AtriumType.label(AtriumColors.onPhoto.withValues(alpha: 0.75))),
                      const SizedBox(height: 8),
                      Text(p.title, style: AtriumType.serifAt(40, color: AtriumColors.onPhoto, height: 1.02).copyWith(letterSpacing: -0.8)),
                    ],
                  ),
                ),
              ],
            );
          },
        ),
      ),
      content: [
        const SizedBox(height: 22),
        const StepsDots(6),
        StoryText(
          g
              ? 'Upright, window end up, at eyebrow level. The arm passes just under the pod, never across it.'
              : 'Upright, window end up, a thumb-width behind the corner of your eye, just above the cheekbone.',
        ),
        const SizedBox(height: 14),
        Text('Fresh pad first: press the pod into the card for three seconds, then lift.', style: AtriumType.secondary(t.ink2).copyWith(fontSize: 15)),
        const SizedBox(height: 26),
        Segmented<WearSide>(
          values: WearSide.values,
          labels: [for (final w in WearSide.values) w.label],
          selected: s.side,
          onChanged: (w) => ref.read(appStateProvider.notifier).update((st) => st.copyWith(side: w)),
        ),
        Padding(
          padding: const EdgeInsets.only(top: 8, left: 4),
          child: Text('Start on the side you sleep on less.', style: AtriumType.caption(t.ink3)),
        ),
        const Spacer(),
      ],
      footer: [PrimaryButton("It's in place", onPressed: () => context.push('/signal'))],
    );
  }

  /// Where an image point lands in a [BoxFit.cover] box.
  static Offset _coverPoint(Size box, Size image, Alignment a, Offset frac) {
    final k = math.max(box.width / image.width, box.height / image.height);
    final w = image.width * k;
    final h = image.height * k;
    final dx = (box.width - w) * (a.x + 1) / 2;
    final dy = (box.height - h) * (a.y + 1) / 2;
    return Offset(dx + frac.dx * w, dy + frac.dy * h);
  }
}

/// A fine ring on the pod with a hairline up to a small frosted label, like a note on a drawing.
class _PodMarker extends StatelessWidget {
  const _PodMarker({required this.at});
  final Offset at;

  @override
  Widget build(BuildContext context) {
    const r = 26.0;
    const lead = 22.0;
    return Stack(
      children: [
        Positioned(
          left: at.dx - r,
          top: at.dy - r,
          width: r * 2,
          height: r * 2,
          child: const CustomPaint(painter: _RingPainter()),
        ),
        // Hairline up from the ring to a small frosted label above it (clear of the face and the photo edge).
        Positioned(
          left: at.dx - 0.5,
          top: at.dy - r - lead,
          width: 1,
          height: lead,
          child: ColoredBox(color: AtriumColors.onPhoto.withValues(alpha: 0.9)),
        ),
        Positioned(
          left: at.dx - 60,
          width: 120,
          top: at.dy - r - lead - 26,
          child: Center(
            child: ClipRRect(
              borderRadius: BorderRadius.circular(Radii.pill),
              child: BackdropFilter(
                filter: ImageFilter.blur(sigmaX: 14, sigmaY: 14),
                child: Container(
                  padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
                  color: const Color(0x52000000),
                  child: Text('Window end up', style: AtriumType.caption(AtriumColors.onPhoto).copyWith(fontWeight: FontWeight.w500)),
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }
}

class _RingPainter extends CustomPainter {
  const _RingPainter();

  @override
  void paint(Canvas canvas, Size s) {
    canvas.drawCircle(
      s.center(Offset.zero),
      s.width / 2 - 1,
      Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.2
        ..color = AtriumColors.onPhoto.withValues(alpha: 0.95),
    );
  }

  @override
  bool shouldRepaint(_RingPainter old) => false;
}
