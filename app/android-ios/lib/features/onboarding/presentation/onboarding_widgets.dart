// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_svg/flutter_svg.dart';

/// The Atrium mark (prototype `MARK`): the hall outline, its floor, and the light beam with a pulse.
String atriumMarkSvg({String ink = '#FFFFFF', String beam = '#FF6F5C', double w = 7}) =>
    '<svg viewBox="140 190 320 280" xmlns="http://www.w3.org/2000/svg"><g fill="none" stroke-linecap="round" stroke-linejoin="round">'
    '<path d="M292 270 L308 270 L362 438 L238 438 Z" fill="$beam" fill-opacity="0.1" stroke="none"/>'
    '<path d="M178 452 V306 C178 226 246 198 290 262" stroke="$ink" stroke-width="$w"/>'
    '<path d="M422 452 V306 C422 226 354 198 310 262" stroke="$ink" stroke-width="$w"/>'
    '<path d="M214 452 V318" stroke="$ink" stroke-width="${w * 0.45}" stroke-opacity="0.55"/>'
    '<path d="M386 452 V318" stroke="$ink" stroke-width="${w * 0.45}" stroke-opacity="0.55"/>'
    '<path d="M150 452 H450" stroke="$ink" stroke-width="$w"/>'
    '<path d="M232 420 H258 C270 420 276 368 290 368 C304 368 306 404 314 402 C322 400 324 390 332 393 C341 397 344 420 356 420 H368" stroke="$beam" stroke-width="${w * 0.72}"/>'
    '</g></svg>';

class AtriumMark extends StatelessWidget {
  const AtriumMark({this.width = 58, super.key});
  final double width;

  @override
  Widget build(BuildContext context) => SvgPicture.string(atriumMarkSvg(), width: width, height: width * 280 / 320);
}

/// The Local Forge logo as a small rounded tile. It is a loud poster, so it only appears small and on plain
/// pages (Settings > About, the end of Why Atrium), never on photo covers.
class LocalForgeLogo extends StatelessWidget {
  const LocalForgeLogo({this.size = 30, super.key});
  final double size;

  @override
  Widget build(BuildContext context) => Container(
    width: size,
    height: size,
    foregroundDecoration: BoxDecoration(
      borderRadius: BorderRadius.circular(size * 0.24),
      border: Border.all(color: AtriumColors.of(context).hair, width: 0.5),
    ),
    child: ClipRRect(
      borderRadius: BorderRadius.circular(size * 0.24),
      child: Image.asset('assets/img/local-forge.jpg', fit: BoxFit.cover, semanticLabel: 'Local Forge logo'),
    ),
  );
}

/// Plain onboarding page (`.ob`): optional floating back pill, gutter padding, content that fills
/// the screen so an [ObArt] or [Spacer] can push the [footer] buttons to the bottom.
class OnboardingScaffold extends StatelessWidget {
  const OnboardingScaffold({required this.content, this.footer = const [], this.footerGap = 0, this.back = true, this.topInset, this.header, super.key});

  final List<Widget> content;

  /// Buttons in the `.foot` column (6 px apart).
  final List<Widget> footer;
  final double footerGap;

  /// Frosted back pill, as the prototype's navbar `.back`.
  final bool back;

  /// Space between the status bar and the content (ignored when [header] is set). By default it clears the
  /// back pill, so the step dots sit under it as in the prototype.
  final double? topInset;

  /// Full-bleed widget at the very top (under the status bar), e.g. a photo.
  final Widget? header;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final pad = MediaQuery.paddingOf(context);
    final page = Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        ?header,
        Expanded(
          child: Padding(
            padding: EdgeInsets.fromLTRB(Space.gutter, header == null ? pad.top + (topInset ?? (back ? 56 : 24)) : 0, Space.gutter, pad.bottom + Space.s),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                ...content,
                if (footer.isNotEmpty) ...[
                  SizedBox(height: footerGap),
                  for (var i = 0; i < footer.length; i++) ...[if (i > 0) const SizedBox(height: 6), footer[i]],
                ],
              ],
            ),
          ),
        ),
      ],
    );
    return Scaffold(
      backgroundColor: t.bg,
      body: Stack(
        children: [
          CustomScrollView(slivers: [SliverFillRemaining(hasScrollBody: false, child: page)]),
          // Content scrolls under a soft fade so the back pill never sits on text.
          if (header == null)
            Positioned(
              left: 0,
              right: 0,
              top: 0,
              height: pad.top + 52,
              child: IgnorePointer(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      stops: const [0, 0.7, 1],
                      colors: [t.bg, t.bg.withValues(alpha: 0.92), t.bg.withValues(alpha: 0)],
                    ),
                  ),
                ),
              ),
            ),
          if (back) Positioned(left: 12, top: pad.top + 6, child: const BackPill()),
        ],
      ),
    );
  }
}

/// Full-bleed photo page (`.ob-photo` + `.ob.on-photo`): dark fade to the bottom, content
/// bottom-aligned in [AtriumColors.onPhoto].
class OnPhotoScaffold extends StatelessWidget {
  const OnPhotoScaffold({required this.image, required this.content, this.footer = const [], this.alignment = Alignment.center, super.key});
  final String image;
  final List<Widget> content;
  final List<Widget> footer;
  final Alignment alignment;

  static const Color _base = Color(0xFF060607);

  @override
  Widget build(BuildContext context) {
    final pad = MediaQuery.paddingOf(context);
    return AnnotatedRegion<SystemUiOverlayStyle>(
      value: SystemUiOverlayStyle.light,
      child: Scaffold(
        backgroundColor: _base,
        body: Stack(
          fit: StackFit.expand,
          children: [
            Image.asset(image, fit: BoxFit.cover, alignment: alignment),
            const DecoratedBox(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                  stops: [0, 0.3, 0.55, 0.82, 1],
                  colors: [Color(0x66060607), Color(0x00060607), Color(0x33060607), Color(0xEB060607), _base],
                ),
              ),
            ),
            CustomScrollView(
              slivers: [
                SliverFillRemaining(
                  hasScrollBody: false,
                  child: Padding(
                    padding: EdgeInsets.fromLTRB(Space.gutter, pad.top + 24, Space.gutter, pad.bottom + Space.s),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      mainAxisAlignment: MainAxisAlignment.end,
                      children: [
                        ...content,
                        for (var i = 0; i < footer.length; i++) ...[if (i > 0) const SizedBox(height: 6), footer[i]],
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

/// `.ob h1`: 38 px serif.
class ObTitle extends StatelessWidget {
  const ObTitle(this.text, {this.size = 38, this.color, this.bottom = 12, super.key});
  final String text;
  final double size;
  final Color? color;
  final double bottom;

  @override
  Widget build(BuildContext context) => Padding(
    padding: EdgeInsets.only(bottom: bottom),
    child: Text(
      text,
      style: AtriumType.serifAt(size, color: color ?? AtriumColors.of(context).ink, height: 1.05).copyWith(letterSpacing: -0.02 * size),
    ),
  );
}

/// `.ob p.lede`: 17 px, muted.
class ObLede extends StatelessWidget {
  const ObLede(this.text, {this.color, this.bottom = 22, super.key});
  final String text;
  final Color? color;
  final double bottom;

  @override
  Widget build(BuildContext context) => Padding(
    padding: EdgeInsets.only(bottom: bottom),
    child: Text(text, style: TextStyle(fontSize: 17, height: 1.42, color: color ?? AtriumColors.of(context).ink2)),
  );
}

/// `.ob .art`: takes the free height (at least [minHeight]) and centres its child.
class ObArt extends StatelessWidget {
  const ObArt({required this.child, this.minHeight = 260, this.alignment = Alignment.center, super.key});
  final Widget child;
  final double minHeight;
  final Alignment alignment;

  @override
  Widget build(BuildContext context) => Expanded(
    child: ConstrainedBox(
      constraints: BoxConstraints(minHeight: minHeight),
      child: Align(alignment: alignment, child: child),
    ),
  );
}

/// Product cutout (`.cut`): the image centred in a band of [height].
class ObCutout extends StatelessWidget {
  const ObCutout(this.image, {required this.widthFactor, required this.maxWidth, this.height = 260, super.key});
  final String image;
  final double widthFactor;
  final double maxWidth;
  final double height;

  @override
  Widget build(BuildContext context) {
    final w = ((MediaQuery.sizeOf(context).width - 2 * Space.gutter) * widthFactor).clamp(0.0, maxWidth);
    return SizedBox(
      height: height,
      child: Center(child: Image.asset(image, width: w)),
    );
  }
}

/// Small pill button (`.btn.sm`).
class ObPillButton extends StatelessWidget {
  const ObPillButton(this.label, {required this.onPressed, super.key});
  final String label;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Pressable(
      onTap: onPressed,
      child: Container(
        height: 36,
        padding: const EdgeInsets.symmetric(horizontal: 16),
        alignment: Alignment.center,
        decoration: BoxDecoration(color: t.button, borderRadius: BorderRadius.circular(Radii.pill)),
        child: Text(label, style: AtriumType.button(t.buttonInk).copyWith(fontSize: 15)),
      ),
    );
  }
}

/// Bottom sheet from the prototype's `SHEETS`: grab bar, serif title, muted paragraphs, one button.
Future<void> showObSheet(BuildContext context, {required String title, required List<String> paragraphs, String button = 'OK', VoidCallback? onButton}) {
  final t = AtriumColors.of(context);
  return showModalBottomSheet<void>(
    context: context,
    isScrollControlled: true,
    backgroundColor: t.cardSolid,
    barrierColor: t.scrim,
    shape: const RoundedRectangleBorder(borderRadius: BorderRadius.vertical(top: Radius.circular(22))),
    builder: (ctx) => SafeArea(
      top: false,
      child: Padding(
        padding: const EdgeInsets.fromLTRB(20, 8, 20, 40),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Center(
              child: Container(
                width: 36,
                height: 5,
                decoration: BoxDecoration(color: t.hair, borderRadius: BorderRadius.circular(3)),
              ),
            ),
            const SizedBox(height: 20),
            Text(title, style: AtriumType.serifAt(28, color: t.ink, height: 1.12)),
            const SizedBox(height: 8),
            for (final p in paragraphs)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(p, style: AtriumType.body(t.ink2)),
              ),
            PrimaryButton(
              button,
              onPressed: () {
                Navigator.of(ctx).pop();
                onButton?.call();
              },
            ),
          ],
        ),
      ),
    ),
  );
}
