// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:ui';

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';

/// Detail page in the Today language: the page's own photo as a cover with the reading on it,
/// then a sheet with one serif sentence and the charts (Recovery, Heart, Sleep, Effort, Skin temp).
class CoverPage extends StatelessWidget {
  const CoverPage({
    required this.image,
    required this.title,
    required this.subtitle,
    required this.reading,
    required this.story,
    required this.children,
    this.alignment = const Alignment(0, -0.4),
    this.backLabel = 'Today',
    super.key,
  });

  final String image;
  final String title;
  final String subtitle;

  /// Gauge or number shown on the cover, right side.
  final Widget reading;
  final String story;
  final List<Widget> children;
  final Alignment alignment;
  final String backLabel;

  @override
  Widget build(BuildContext context) => _CoverBody(page: this);
}

class _CoverBody extends StatefulWidget {
  const _CoverBody({required this.page});
  final CoverPage page;

  @override
  State<_CoverBody> createState() => _CoverBodyState();
}

class _CoverBodyState extends State<_CoverBody> {
  static const double _cover = 470;
  bool _pastPhoto = false;

  bool _onScroll(ScrollNotification n) {
    if (n.metrics.axis != Axis.vertical) return false;
    final past = n.metrics.pixels > _cover - 22 - MediaQuery.paddingOf(context).top;
    if (past != _pastPhoto) setState(() => _pastPhoto = past);
    return false;
  }

  @override
  Widget build(BuildContext context) {
    final CoverPage(:image, :alignment, :title, :subtitle, :reading, :story, :children, :backLabel) = widget.page;
    final t = AtriumColors.of(context);
    final top = MediaQuery.paddingOf(context).top;
    final dark = Theme.of(context).brightness == Brightness.dark;
    return AnnotatedRegion<SystemUiOverlayStyle>(
      // Light status icons on the photo; the theme's once the sheet reaches the top.
      value: !_pastPhoto || dark ? SystemUiOverlayStyle.light : SystemUiOverlayStyle.dark,
      child: Scaffold(
        backgroundColor: t.bg,
        body: Stack(
          children: [
            NotificationListener<ScrollNotification>(
              onNotification: _onScroll,
              child: ListView(
                padding: EdgeInsets.zero,
                children: [
                  SizedBox(
                    height: _cover,
                    child: Stack(
                      fit: StackFit.expand,
                      children: [
                        Image.asset('assets/img/$image.jpg', fit: BoxFit.cover, alignment: alignment),
                        const DecoratedBox(
                          decoration: BoxDecoration(
                            gradient: LinearGradient(
                              begin: Alignment.topCenter,
                              end: Alignment.bottomCenter,
                              stops: [0, 0.2, 0.42, 1],
                              colors: [Color(0x61000000), Color(0x00000000), Color(0x00000000), Color(0x9E000000)],
                            ),
                          ),
                        ),
                        Positioned(
                          left: 20,
                          right: 20,
                          bottom: 34,
                          child: Row(
                            crossAxisAlignment: CrossAxisAlignment.end,
                            children: [
                              Expanded(
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  mainAxisSize: MainAxisSize.min,
                                  children: [
                                    Text(title, style: AtriumType.serifAt(40, color: Colors.white, height: 1)),
                                    const SizedBox(height: 8),
                                    Text(subtitle, style: TextStyle(fontSize: 15, color: Colors.white.withValues(alpha: 0.82))),
                                  ],
                                ),
                              ),
                              reading,
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                  Transform.translate(
                    offset: const Offset(0, -22),
                    child: Container(
                      decoration: BoxDecoration(
                        color: t.bg,
                        borderRadius: const BorderRadius.vertical(top: Radius.circular(Radii.sheet)),
                      ),
                      padding: EdgeInsets.fromLTRB(Space.gutter, 26, Space.gutter, MediaQuery.paddingOf(context).bottom + 110),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Reveal(
                            child: Padding(padding: const EdgeInsets.fromLTRB(4, 0, 4, 20), child: StoryText(story)),
                          ),
                          for (var i = 0; i < children.length; i++) Reveal(order: i + 1, child: children[i]),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
            ),
            if (_pastPhoto) StatusBarFrost(height: top),
            Positioned(
              left: 12,
              top: top + 6,
              child: BackPill(label: backLabel),
            ),
          ],
        ),
      ),
    );
  }
}

/// White cover reading: a big number with its unit.
class CoverNumber extends StatelessWidget {
  const CoverNumber(this.value, this.unit, {super.key});
  final String value;
  final String unit;

  @override
  Widget build(BuildContext context) {
    // Whole numbers count up as the page opens, like the gauges; anything else (e.g. "+0.2") just shows.
    final n = int.tryParse(value);
    if (n == null || Motion.reduced(context)) return _text(value);
    return TweenAnimationBuilder<double>(
      tween: Tween(begin: 0, end: n.toDouble()),
      duration: Motion.gauge,
      curve: Motion.ease,
      builder: (_, v, _) => _text('${v.round()}'),
    );
  }

  Widget _text(String value) => Text.rich(
    TextSpan(
      children: [
        TextSpan(
          text: value,
          style: AtriumType.coverNumber(Colors.white).copyWith(
            shadows: const [Shadow(color: Color(0x40000000), blurRadius: 24, offset: Offset(0, 2))],
          ),
        ),
        TextSpan(
          text: ' $unit',
          style: TextStyle(fontSize: 16, fontWeight: FontWeight.w500, color: Colors.white.withValues(alpha: 0.85)),
        ),
      ],
    ),
  );
}

/// White cover reading: the arch gauge.
class CoverArch extends StatelessWidget {
  const CoverArch(this.value, this.color, {this.learning = false, super.key});
  final double value;
  final Color color;
  final bool learning;

  @override
  Widget build(BuildContext context) => SizedBox(
    width: 118,
    child: ArchGauge(value: value, color: color, small: true, strokeWidth: 13, onPhoto: true, learning: learning),
  );
}

/// Plain page with a large serif title (Trends, Device, Settings, Skin pad, Pod software).
class PlainPage extends StatelessWidget {
  const PlainPage({required this.title, required this.children, this.back, this.eyebrow, super.key});
  final String title;
  final List<Widget> children;

  /// Back label; null on tab roots.
  final String? back;
  final String? eyebrow;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final top = MediaQuery.paddingOf(context).top;
    return Scaffold(
      backgroundColor: t.bg,
      body: Stack(
        children: [
          ListView(
            padding: EdgeInsets.fromLTRB(Space.gutter, top + (back != null ? 8 : 24), Space.gutter, MediaQuery.paddingOf(context).bottom + 110),
            children: [
              if (back != null) ...[
                Align(
                  alignment: Alignment.centerLeft,
                  child: Transform.translate(
                    offset: const Offset(-8, 0),
                    child: BackPill(label: back!),
                  ),
                ),
                const SizedBox(height: 10),
              ],
              LargeTitle(title, eyebrow: eyebrow),
              for (var i = 0; i < children.length; i++) Reveal(order: i, child: children[i]),
            ],
          ),
          StatusBarFrost(height: top),
        ],
      ),
    );
  }
}

/// Frosted strip under the status bar so scrolled content never runs into the clock (prototype sticky navbar).
class StatusBarFrost extends StatelessWidget {
  const StatusBarFrost({required this.height, super.key});
  final double height;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Positioned(
      left: 0,
      right: 0,
      top: 0,
      height: height,
      child: ClipRect(
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 20, sigmaY: 20),
          child: ColoredBox(color: t.bg.withValues(alpha: 0.78)),
        ),
      ),
    );
  }
}

/// A photo-topped scroll page: light status icons over the photo; once [photoHeight] has scrolled past,
/// the theme's icons over a [StatusBarFrost].
class PhotoScroll extends StatefulWidget {
  const PhotoScroll({required this.photoHeight, required this.child, super.key});
  final double photoHeight;
  final Widget child;

  @override
  State<PhotoScroll> createState() => _PhotoScrollState();
}

class _PhotoScrollState extends State<PhotoScroll> {
  bool _past = false;

  @override
  Widget build(BuildContext context) {
    final top = MediaQuery.paddingOf(context).top;
    final dark = Theme.of(context).brightness == Brightness.dark;
    return AnnotatedRegion<SystemUiOverlayStyle>(
      value: !_past || dark ? SystemUiOverlayStyle.light : SystemUiOverlayStyle.dark,
      child: Stack(
        children: [
          NotificationListener<ScrollNotification>(
            onNotification: (n) {
              final past = n.metrics.axis == Axis.vertical && n.metrics.pixels > widget.photoHeight - top;
              if (past != _past) setState(() => _past = past);
              return false;
            },
            child: widget.child,
          ),
          if (_past) StatusBarFrost(height: top),
        ],
      ),
    );
  }
}

/// An [IndexedStack] whose newly chosen child fades and settles in; the others keep their state.
class FadeIndexedStack extends StatefulWidget {
  const FadeIndexedStack({required this.index, required this.children, super.key});
  final int index;
  final List<Widget> children;

  @override
  State<FadeIndexedStack> createState() => _FadeIndexedStackState();
}

class _FadeIndexedStackState extends State<FadeIndexedStack> with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(vsync: this, duration: Motion.fade, value: 1);
  late final Animation<double> _t = CurvedAnimation(parent: _c, curve: Motion.ease);

  @override
  void didUpdateWidget(FadeIndexedStack old) {
    super.didUpdateWidget(old);
    if (old.index != widget.index && !Motion.reduced(context)) _c.forward(from: 0);
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
      builder: (_, child) => Transform.translate(offset: Offset(0, 6 * (1 - _t.value)), child: child),
      child: IndexedStack(index: widget.index, children: widget.children),
    ),
  );
}

/// Bottom tab bar, frosted, over every tab root.
class TabShell extends StatelessWidget {
  const TabShell({required this.shell, super.key});
  final StatefulNavigationShell shell;

  static const _tabs = [('today', 'Today'), ('trends', 'Trends'), ('device', 'Device'), ('settings', 'Settings')];

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Scaffold(
      extendBody: true,
      body: shell,
      bottomNavigationBar: ClipRect(
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 24, sigmaY: 24),
          child: Container(
            decoration: BoxDecoration(
              color: t.barBg,
              border: Border(top: BorderSide(color: t.hair, width: 0.5)),
            ),
            padding: EdgeInsets.only(top: 7, bottom: MediaQuery.paddingOf(context).bottom + 4),
            child: Row(
              children: [
                for (var i = 0; i < _tabs.length; i++)
                  Expanded(
                    child: GestureDetector(
                      behavior: HitTestBehavior.opaque,
                      onTap: () {
                        if (i != shell.currentIndex) HapticFeedback.selectionClick();
                        shell.goBranch(i, initialLocation: i == shell.currentIndex);
                      },
                      child: Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          AIcon(_tabs[i].$1, size: 26, color: i == shell.currentIndex ? t.ink : t.ink3),
                          const SizedBox(height: 3),
                          Text(
                            _tabs[i].$2,
                            style: TextStyle(fontSize: 10.5, fontWeight: FontWeight.w500, letterSpacing: 0.2, color: i == shell.currentIndex ? t.ink : t.ink3),
                          ),
                        ],
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
