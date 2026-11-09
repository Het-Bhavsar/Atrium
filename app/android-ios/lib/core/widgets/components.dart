// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:ui';

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';

/// Rounded grey card (#F4F4F6 light, white 5.5 % dark).
class AtriumCard extends StatelessWidget {
  const AtriumCard({required this.child, this.padding = const EdgeInsets.all(Space.l), this.onTap, this.color, super.key});
  final Widget child;
  final EdgeInsetsGeometry padding;
  final VoidCallback? onTap;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final card = Container(
      width: double.infinity,
      padding: padding,
      decoration: BoxDecoration(color: color ?? t.card, borderRadius: BorderRadius.circular(Radii.card)),
      child: child,
    );
    return onTap == null ? card : Pressable(onTap: onTap!, child: card);
  }
}

/// Scale-on-press, like the prototype's `.card.tap:active`.
class Pressable extends StatefulWidget {
  const Pressable({required this.child, required this.onTap, super.key});
  final Widget child;
  final VoidCallback onTap;

  @override
  State<Pressable> createState() => _PressableState();
}

class _PressableState extends State<Pressable> {
  bool _down = false;

  @override
  Widget build(BuildContext context) => GestureDetector(
    behavior: HitTestBehavior.opaque,
    onTapDown: (_) => setState(() => _down = true),
    onTapCancel: () => setState(() => _down = false),
    onTapUp: (_) => setState(() => _down = false),
    onTap: widget.onTap,
    child: AnimatedScale(scale: _down ? 0.98 : 1, duration: Motion.of(context, Motion.tap), curve: Motion.ease, child: widget.child),
  );
}

class SectionHeader extends StatelessWidget {
  const SectionHeader(this.title, {this.trailing, this.onTrailing, super.key});
  final String title;
  final String? trailing;
  final VoidCallback? onTrailing;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Padding(
      padding: const EdgeInsets.fromLTRB(4, 26, 4, 10),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.baseline,
        textBaseline: TextBaseline.alphabetic,
        children: [
          Expanded(child: Text(title, style: AtriumType.section(t.ink))),
          if (trailing != null)
            GestureDetector(
              onTap: onTrailing,
              child: Text(trailing!, style: AtriumType.secondary(t.ink2)),
            ),
        ],
      ),
    );
  }
}

/// One serif sentence that says what the numbers mean (Today's voice).
class StoryText extends StatelessWidget {
  const StoryText(this.text, {this.center = false, this.color, super.key});
  final String text;
  final bool center;
  final Color? color;

  @override
  Widget build(BuildContext context) =>
      Text(text, textAlign: center ? TextAlign.center : TextAlign.start, style: AtriumType.story(color ?? AtriumColors.of(context).ink));
}

class FootNote extends StatelessWidget {
  const FootNote(this.text, {this.center = false, super.key});
  final String text;
  final bool center;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.fromLTRB(4, 22, 4, 0),
    child: Text(text, textAlign: center ? TextAlign.center : TextAlign.start, style: AtriumType.caption(AtriumColors.of(context).ink3).copyWith(fontSize: 13)),
  );
}

class LargeTitle extends StatelessWidget {
  const LargeTitle(this.title, {this.eyebrow, this.trailing, this.color, super.key});
  final String title;
  final String? eyebrow;
  final Widget? trailing;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final ink = color ?? t.ink;
    return Padding(
      padding: const EdgeInsets.only(top: 6, bottom: 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (eyebrow != null) ...[
            Text(eyebrow!.toUpperCase(), style: AtriumType.label(color != null ? ink.withValues(alpha: 0.78) : t.ink2)),
            const SizedBox(height: 8),
          ],
          Row(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              Expanded(child: Text(title, style: AtriumType.largeTitle(ink))),
              ?trailing,
            ],
          ),
        ],
      ),
    );
  }
}

class PrimaryButton extends StatelessWidget {
  const PrimaryButton(this.label, {required this.onPressed, this.onPhoto = false, super.key});
  final String label;
  final VoidCallback? onPressed;
  final bool onPhoto;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final enabled = onPressed != null;
    final bg = onPhoto ? AtriumColors.onPhoto : t.button;
    final fg = onPhoto ? const Color(0xFF121212) : t.buttonInk;
    return Pressable(
      onTap: onPressed ?? () {},
      child: AnimatedContainer(
        duration: Motion.tap,
        height: 50,
        alignment: Alignment.center,
        decoration: BoxDecoration(color: enabled ? bg : t.ink3, borderRadius: BorderRadius.circular(Radii.pill)),
        child: Text(label, style: AtriumType.button(enabled ? fg : t.bg)),
      ),
    );
  }
}

class PlainButton extends StatelessWidget {
  const PlainButton(this.label, {required this.onPressed, this.color, super.key});
  final String label;
  final VoidCallback onPressed;
  final Color? color;

  @override
  Widget build(BuildContext context) => GestureDetector(
    onTap: onPressed,
    behavior: HitTestBehavior.opaque,
    child: SizedBox(
      height: 46,
      child: Center(child: Text(label, style: AtriumType.button(color ?? AtriumColors.of(context).ink).copyWith(fontSize: 16))),
    ),
  );
}

/// Pill segmented control.
class Segmented<T> extends StatelessWidget {
  const Segmented({required this.values, required this.labels, required this.selected, required this.onChanged, super.key});
  final List<T> values;
  final List<String> labels;
  final T selected;
  final ValueChanged<T> onChanged;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final n = values.length;
    final i = values.indexOf(selected).clamp(0, n - 1);
    final d = Motion.of(context, Motion.page);
    return Container(
      height: 36,
      padding: const EdgeInsets.all(3),
      decoration: BoxDecoration(color: t.track, borderRadius: BorderRadius.circular(Radii.pill)),
      child: Stack(
        children: [
          // The thumb slides to the chosen segment instead of jumping.
          AnimatedAlign(
            duration: d,
            curve: Motion.ease,
            alignment: Alignment(n == 1 ? 0 : -1 + 2 * i / (n - 1), 0),
            child: FractionallySizedBox(
              widthFactor: 1 / n,
              heightFactor: 1,
              child: DecoratedBox(
                decoration: BoxDecoration(
                  color: t.cardSolid,
                  borderRadius: BorderRadius.circular(Radii.pill),
                  boxShadow: const [BoxShadow(color: Color(0x24000000), blurRadius: 3, offset: Offset(0, 1))],
                ),
              ),
            ),
          ),
          Row(
            children: [
              for (var k = 0; k < n; k++)
                Expanded(
                  child: Semantics(
                    button: true,
                    selected: k == i,
                    child: GestureDetector(
                      behavior: HitTestBehavior.opaque,
                      onTap: () {
                        if (k != i) HapticFeedback.selectionClick();
                        onChanged(values[k]);
                      },
                      child: Center(
                        child: AnimatedDefaultTextStyle(
                          duration: d,
                          curve: Motion.ease,
                          style: TextStyle(fontSize: 13, fontWeight: k == i ? FontWeight.w600 : FontWeight.w500, color: k == i ? t.ink : t.ink2),
                          child: Text(labels[k]),
                        ),
                      ),
                    ),
                  ),
                ),
            ],
          ),
        ],
      ),
    );
  }
}

class Meter extends StatelessWidget {
  const Meter(this.fraction, {this.color, this.height = 6, super.key});
  final double fraction;
  final Color? color;
  final double height;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return ClipRRect(
      borderRadius: BorderRadius.circular(height / 2),
      child: Container(
        height: height,
        color: t.track,
        alignment: Alignment.centerLeft,
        child: AnimatedFractionallySizedBox(
          duration: Motion.of(context, Motion.meter),
          curve: Motion.ease,
          widthFactor: fraction.clamp(0, 1),
          child: Container(
            decoration: BoxDecoration(color: color ?? t.green, borderRadius: BorderRadius.circular(height / 2)),
          ),
        ),
      ),
    );
  }
}

/// Grouped list (iOS-style), with hairlines between rows.
class ListGroup extends StatelessWidget {
  const ListGroup({required this.children, super.key});
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return ClipRRect(
      borderRadius: BorderRadius.circular(Radii.card),
      child: ColoredBox(
        color: t.card,
        child: Column(
          children: [
            for (var i = 0; i < children.length; i++) ...[if (i > 0) Divider(height: 0.5, thickness: 0.5, indent: 16, color: t.hair), children[i]],
          ],
        ),
      ),
    );
  }
}

class ListRow extends StatelessWidget {
  const ListRow({
    required this.title,
    this.subtitle,
    this.icon,
    this.iconColor,
    this.leading,
    this.value,
    this.trailing,
    this.chevron = false,
    this.danger = false,
    this.onTap,
    super.key,
  });

  final String title;
  final String? subtitle;
  final String? icon;
  final Color? iconColor;

  /// Shown in place of the [icon] chip, at the same 30 px size (for example a logo tile).
  final Widget? leading;
  final String? value;
  final Widget? trailing;
  final bool chevron;
  final bool danger;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final row = Padding(
      padding: const EdgeInsets.symmetric(horizontal: Space.l, vertical: 13),
      child: Row(
        children: [
          if (leading != null) ...[
            leading!,
            const SizedBox(width: Space.m),
          ] else if (icon != null) ...[
            Container(
              width: 30,
              height: 30,
              decoration: BoxDecoration(color: t.track, borderRadius: BorderRadius.circular(15)),
              alignment: Alignment.center,
              child: AIcon(icon!, size: 17, color: iconColor ?? t.ink2),
            ),
            const SizedBox(width: Space.m),
          ],
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(title, style: AtriumType.body(danger ? t.red : t.ink)),
                if (subtitle != null)
                  Padding(
                    padding: const EdgeInsets.only(top: 2),
                    child: Text(subtitle!, style: AtriumType.secondary(t.ink2).copyWith(fontSize: 13.5, height: 1.38)),
                  ),
              ],
            ),
          ),
          if (value != null)
            Padding(
              padding: const EdgeInsets.only(left: Space.s),
              child: Text(value!, style: AtriumType.body(t.ink2)),
            ),
          ?trailing,
          if (chevron)
            Padding(
              padding: const EdgeInsets.only(left: 6),
              child: AIcon('chev', size: 16, color: t.ink3),
            ),
        ],
      ),
    );
    return onTap == null ? row : InkWell(onTap: onTap, child: row);
  }
}

/// Tile on Today and detail pages: label, big number, note, sparkline.
class MetricTile extends StatelessWidget {
  const MetricTile({
    required this.icon,
    required this.name,
    required this.color,
    required this.value,
    required this.unit,
    required this.note,
    this.spark,
    this.band,
    this.onTap,
    super.key,
  });

  final String icon;
  final String name;
  final Color color;
  final String value;
  final String unit;
  final String note;
  final List<double>? spark;
  final (double, double)? band;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return AtriumCard(
      onTap: onTap,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              AIcon(icon, size: 14, color: color, stroke: 2),
              const SizedBox(width: 5),
              Text(name.toUpperCase(), style: AtriumType.label(color)),
            ],
          ),
          const SizedBox(height: 10),
          BigValue(value, unit),
          const SizedBox(height: 6),
          Text(note, style: AtriumType.secondary(t.ink2).copyWith(fontSize: 13.5)),
          if (spark != null) ...[const SizedBox(height: 10), Sparkline(values: spark!, color: color, band: band)],
        ],
      ),
    );
  }
}

class BigValue extends StatelessWidget {
  const BigValue(this.value, this.unit, {this.size = 34, this.color, super.key});
  final String value;
  final String unit;
  final double size;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Text.rich(
      TextSpan(
        children: [
          TextSpan(
            text: value,
            style: AtriumType.serifAt(size, weight: 300, color: color ?? t.ink, height: 1),
          ),
          if (unit.isNotEmpty) TextSpan(text: ' $unit', style: AtriumType.unit(color?.withValues(alpha: 0.85) ?? t.ink2)),
        ],
      ),
    );
  }
}

/// Card with an icon dot, a bold title and a muted line.
class NoticeCard extends StatelessWidget {
  const NoticeCard({required this.icon, required this.title, required this.body, this.iconColor, this.onTap, super.key});
  final String icon;
  final String title;
  final String body;
  final Color? iconColor;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: Space.m),
      child: AtriumCard(
        onTap: onTap,
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Container(
              width: 30,
              height: 30,
              decoration: BoxDecoration(color: t.track, borderRadius: BorderRadius.circular(15)),
              alignment: Alignment.center,
              child: AIcon(icon, size: 17, color: iconColor ?? t.ink2),
            ),
            const SizedBox(width: Space.m),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(title, style: AtriumType.bodyStrong(t.ink)),
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

class StepsDots extends StatelessWidget {
  const StepsDots(this.index, {this.count = 8, this.onPhoto = false, super.key});
  final int index;
  final int count;
  final bool onPhoto;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final on = onPhoto ? AtriumColors.onPhoto : t.ink;
    return Padding(
      padding: const EdgeInsets.only(bottom: 14),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          for (var i = 0; i < count; i++)
            AnimatedContainer(
              duration: Motion.tap,
              margin: const EdgeInsets.symmetric(horizontal: 2.5),
              width: i == index ? 18 : 5,
              height: 5,
              decoration: BoxDecoration(color: i == index ? on : on.withValues(alpha: 0.18), borderRadius: BorderRadius.circular(3)),
            ),
        ],
      ),
    );
  }
}

/// Battery pill: "● 60%". Taps through to Device.
class PodChip extends StatelessWidget {
  const PodChip(this.status, {this.onPhoto = false, this.onTap, super.key});
  final PodStatus status;
  final bool onPhoto;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final fg = onPhoto ? AtriumColors.onPhoto : t.ink;
    final chip = Container(
      padding: const EdgeInsets.fromLTRB(8, 5, 10, 5),
      decoration: BoxDecoration(
        color: onPhoto ? const Color(0x59141416) : t.card,
        borderRadius: BorderRadius.circular(Radii.pill),
        border: onPhoto ? Border.all(color: Colors.white.withValues(alpha: 0.25), width: 0.5) : null,
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 6,
            height: 6,
            decoration: BoxDecoration(shape: BoxShape.circle, color: status.connected ? t.green : t.ink3),
          ),
          const SizedBox(width: 6),
          if (status.charging) AIcon('bolt', size: 13, color: t.green, stroke: 2),
          Text(
            status.connected ? '${status.batteryPercent}%' : 'Not connected',
            style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600, color: fg),
          ),
        ],
      ),
    );
    final tappable = GestureDetector(onTap: onTap ?? () => context.go('/device'), child: chip);
    if (!onPhoto) return tappable;
    return ClipRRect(
      borderRadius: BorderRadius.circular(Radii.pill),
      child: BackdropFilter(filter: ImageFilter.blur(sigmaX: 18, sigmaY: 18), child: tappable),
    );
  }
}

/// Floating back pill used over photos and on plain pages.
class BackPill extends StatelessWidget {
  const BackPill({this.label = 'Back', super.key});
  final String label;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final fg = t.ink;
    return GestureDetector(
      onTap: () => context.canPop() ? context.pop() : context.go('/today'),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(Radii.pill),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 20, sigmaY: 20),
          child: Container(
            padding: const EdgeInsets.fromLTRB(8, 6, 12, 6),
            // Always the frosted bar fill and hairline edge (prototype .navbar .back), so it stays readable over content.
            decoration: BoxDecoration(
              color: t.bg.withValues(alpha: 0.72),
              borderRadius: BorderRadius.circular(Radii.pill),
              border: Border.all(color: t.hair, width: 0.5),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                AIcon('back', size: 18, color: fg, stroke: 2.2),
                const SizedBox(width: 2),
                Text(label, style: TextStyle(fontSize: 16, color: fg)),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// Fades content up into place when it first appears, a step after the item before it ([order]).
/// Plays once per page; nothing moves when the system asks for less motion.
class Reveal extends StatefulWidget {
  const Reveal({required this.child, this.order = 0, super.key});
  final Widget child;
  final int order;

  @override
  State<Reveal> createState() => _RevealState();
}

class _RevealState extends State<Reveal> with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(vsync: this, duration: Motion.reveal);
  late final Animation<double> _t = CurvedAnimation(parent: _c, curve: Motion.ease);

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (_c.status != AnimationStatus.dismissed) return;
    if (Motion.reduced(context)) {
      _c.value = 1;
    } else {
      Future.delayed(Duration(milliseconds: 45 * widget.order.clamp(0, 6)), () {
        if (mounted) _c.forward();
      });
    }
  }

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => FadeTransition(
    opacity: _t,
    child: AnimatedBuilder(
      animation: _t,
      builder: (_, child) => Transform.translate(offset: Offset(0, 12 * (1 - _t.value)), child: child),
      child: widget.child,
    ),
  );
}
