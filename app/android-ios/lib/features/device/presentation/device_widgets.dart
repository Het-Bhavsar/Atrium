// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:math' as math;

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:flutter/material.dart';

/// Product cutout on a soft card (the prototype's `.cut-card`), with an optional caption.
class CutCard extends StatelessWidget {
  const CutCard({
    required this.image,
    required this.semanticLabel,
    this.caption,
    this.dim = false,
    this.maxImageHeight = 200,
    this.widthFactor = 0.7,
    super.key,
  });
  final String image;
  final String semanticLabel;
  final Widget? caption;

  /// Greyscale and faded, for a pod that isn't connected.
  final bool dim;
  final double maxImageHeight;
  final double widthFactor;

  static const List<double> _greyscale = [0.2126, 0.7152, 0.0722, 0, 0, 0.2126, 0.7152, 0.0722, 0, 0, 0.2126, 0.7152, 0.0722, 0, 0, 0, 0, 0, 1, 0];

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    Widget img = Image.asset('assets/img/$image.png', fit: BoxFit.contain, semanticLabel: semanticLabel);
    if (dim) {
      img = Opacity(
        opacity: 0.45,
        child: ColorFiltered(colorFilter: const ColorFilter.matrix(_greyscale), child: img),
      );
    }
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.fromLTRB(Space.l, 18, Space.l, Space.l),
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(Radii.card),
        border: Border.all(color: t.hair, width: 0.5),
        gradient: RadialGradient(center: const Alignment(0, -0.6), radius: 1.2, colors: [t.cardSolid, t.card]),
      ),
      child: Column(
        children: [
          LayoutBuilder(
            builder: (context, c) => ConstrainedBox(
              constraints: BoxConstraints(maxWidth: widthFactor >= 1 ? c.maxWidth : math.min(c.maxWidth * widthFactor, 260), maxHeight: maxImageHeight),
              child: img,
            ),
          ),
          if (caption != null)
            Padding(
              padding: const EdgeInsets.only(top: 6),
              child: SizedBox(width: double.infinity, child: caption),
            ),
        ],
      ),
    );
  }
}

/// Full-width grey pill (the prototype's `.btn.grey`).
class GreyButton extends StatelessWidget {
  const GreyButton(this.label, {required this.onPressed, super.key});
  final String label;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Pressable(
      onTap: onPressed,
      child: Container(
        height: 50,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: t.card,
          borderRadius: BorderRadius.circular(Radii.pill),
          border: Border.all(color: t.hair, width: 0.5),
        ),
        child: Text(label, style: AtriumType.button(t.ink)),
      ),
    );
  }
}

/// Full-width red pill for destructive confirmations.
class DangerButton extends StatelessWidget {
  const DangerButton(this.label, {required this.onPressed, super.key});
  final String label;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Pressable(
      onTap: onPressed,
      child: Container(
        height: 50,
        alignment: Alignment.center,
        decoration: BoxDecoration(color: t.red, borderRadius: BorderRadius.circular(Radii.pill)),
        child: Text(label, style: AtriumType.button(t.buttonInk)),
      ),
    );
  }
}

/// Small muted note under a list (the prototype's `.list-f`).
class ListFoot extends StatelessWidget {
  const ListFoot(this.text, {super.key});
  final String text;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.fromLTRB(Space.l, 9, Space.l, 0),
    child: Text(text, style: AtriumType.secondary(AtriumColors.of(context).ink2).copyWith(fontSize: 13.5, height: 1.45)),
  );
}

/// Brief confirmation from Atrium (the prototype's banner toast).
void showAtriumToast(BuildContext context, String text) {
  final t = AtriumColors.of(context);
  final messenger = ScaffoldMessenger.maybeOf(context);
  if (messenger == null) return;
  messenger
    ..hideCurrentSnackBar()
    ..showSnackBar(
      SnackBar(
        behavior: SnackBarBehavior.floating,
        backgroundColor: t.cardSolid,
        elevation: 6,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(Radii.small)),
        duration: const Duration(milliseconds: 5200),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Atrium', style: AtriumType.secondary(t.ink).copyWith(fontWeight: FontWeight.w600)),
            const SizedBox(height: 2),
            Text(text, style: AtriumType.secondary(t.ink2)),
          ],
        ),
      ),
    );
}
