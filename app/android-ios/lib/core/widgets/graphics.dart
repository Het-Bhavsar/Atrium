// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/icon_paths.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_svg/flutter_svg.dart';

/// Stroke icon from the prototype set.
class AIcon extends StatelessWidget {
  const AIcon(this.name, {this.size = 22, this.color, this.stroke = 1.7, super.key});
  final String name;
  final double size;
  final Color? color;
  final double stroke;

  @override
  Widget build(BuildContext context) {
    final c = color ?? DefaultTextStyle.of(context).style.color ?? AtriumColors.of(context).ink;
    final svg =
        '<svg viewBox="0 0 24 24" fill="none" stroke="#000" stroke-width="$stroke" '
        'stroke-linecap="round" stroke-linejoin="round">${kIconPaths[name] ?? ''}</svg>';
    return SvgPicture.string(svg, width: size, height: size, colorFilter: ColorFilter.mode(c, BlendMode.srcIn));
  }
}

/// The arch gauge: the hall outline from the Atrium mark, filled along its length.
class ArchGauge extends StatefulWidget {
  const ArchGauge({
    required this.value,
    required this.color,
    this.label,
    this.inner,
    this.number,
    this.learning = false,
    this.noFill = false,
    this.small = false,
    this.strokeWidth = 11,
    this.onPhoto = false,
    super.key,
  });

  /// 0-100.
  final double value;
  final Color color;
  final String? label;

  /// Small caps text under the number, e.g. "RECOVERY".
  final String? inner;

  /// Overrides the number shown (e.g. live bpm).
  final String? number;
  final bool learning;
  final bool noFill;
  final bool small;
  final double strokeWidth;
  final bool onPhoto;

  @override
  State<ArchGauge> createState() => _ArchGaugeState();
}

class _ArchGaugeState extends State<ArchGauge> with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(vsync: this, duration: Motion.gauge)..forward();

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (Motion.reduced(context)) _c.value = 1;
  }

  @override
  void didUpdateWidget(covariant ArchGauge old) {
    super.didUpdateWidget(old);
    if (old.value != widget.value) _c.forward(from: 0);
  }

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final ink = widget.onPhoto ? AtriumColors.onPhoto : t.ink;
    final track = widget.onPhoto ? Colors.white.withValues(alpha: 0.24) : t.track;
    final text = widget.learning ? '–' : (widget.number ?? widget.value.round().toString());
    return Semantics(
      label: '${widget.label ?? ''} ${widget.learning ? 'learning' : text}',
      child: AspectRatio(
        aspectRatio: 200 / 182,
        child: AnimatedBuilder(
          animation: _c,
          builder: (_, _) => CustomPaint(
            painter: _ArchPainter(
              fraction: widget.learning || widget.noFill ? 0 : (widget.value / 100) * Motion.ease.transform(_c.value),
              color: widget.color,
              track: track,
              floor: widget.onPhoto ? Colors.white.withValues(alpha: 0.35) : t.hair,
              stroke: widget.strokeWidth,
              dotted: widget.learning,
              glow: t.glow,
            ),
            child: LayoutBuilder(
              builder: (context, box) {
                final k = box.maxWidth / 200;
                return Stack(
                  children: [
                    Positioned(
                      left: 0,
                      right: 0,
                      top: (widget.small ? 72 : 66) * k,
                      child: Text(
                        // The number counts up with the arc.
                        widget.learning || widget.number != null ? text : (widget.value * Motion.ease.transform(_c.value)).round().toString(),
                        textAlign: TextAlign.center,
                        style: AtriumType.serifAt((widget.small ? 60 : 62) * k, weight: 300, color: ink, height: 1),
                      ),
                    ),
                    if (widget.inner != null)
                      Positioned(
                        left: 0,
                        right: 0,
                        top: 148 * k,
                        child: Text(
                          widget.inner!,
                          textAlign: TextAlign.center,
                          style: AtriumType.label(widget.onPhoto ? AtriumColors.onPhoto.withValues(alpha: 0.82) : t.ink2)
                              .copyWith(fontSize: 12 * k * 1.0, letterSpacing: 2.2 * k),
                        ),
                      ),
                  ],
                );
              },
            ),
          ),
        ),
      ),
    );
  }
}

class _ArchPainter extends CustomPainter {
  _ArchPainter({
    required this.fraction,
    required this.color,
    required this.track,
    required this.floor,
    required this.stroke,
    required this.dotted,
    required this.glow,
  });

  final double fraction;
  final Color color;
  final Color track;
  final Color floor;
  final double stroke;
  final bool dotted;
  final double glow;

  @override
  void paint(Canvas canvas, Size size) {
    final k = size.width / 200;
    canvas.scale(k);
    final path = Path()
      ..moveTo(22, 168)
      ..lineTo(22, 86)
      ..arcToPoint(const Offset(178, 86), radius: const Radius.circular(78))
      ..lineTo(178, 168);
    final p = Paint()
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round
      ..strokeWidth = stroke;
    if (dotted) {
      final m = path.computeMetrics().first;
      for (var d = 0.0; d < m.length; d += 13) {
        canvas.drawPath(m.extractPath(d, d + 1), p..color = track);
      }
    } else {
      canvas.drawPath(path, p..color = track);
    }
    if (fraction > 0) {
      final m = path.computeMetrics().first;
      final seg = m.extractPath(0, m.length * fraction);
      canvas
        ..drawPath(
          seg,
          Paint()
            ..style = PaintingStyle.stroke
            ..strokeCap = StrokeCap.round
            ..strokeWidth = stroke * 1.6
            ..color = color.withValues(alpha: 0.35 * glow)
            ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 6),
        )
        ..drawPath(seg, p..color = color);
    }
    canvas.drawLine(
      const Offset(4, 178),
      const Offset(196, 178),
      Paint()
        ..color = floor
        ..strokeWidth = 1,
    );
  }

  @override
  bool shouldRepaint(_ArchPainter o) => o.fraction != fraction || o.color != color || o.track != track;
}

/// Small trend line used on tiles and rows.
class Sparkline extends StatelessWidget {
  const Sparkline({required this.values, required this.color, this.height = 30, this.band, super.key});
  final List<double> values;
  final Color color;
  final double height;
  final (double, double)? band;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: height,
    width: double.infinity,
    child: CustomPaint(painter: _SparkPainter(values, color, band, AtriumColors.of(context).card)),
  );
}

class _SparkPainter extends CustomPainter {
  _SparkPainter(this.v, this.color, this.band, this.bg);
  final List<double> v;
  final Color color;
  final (double, double)? band;
  final Color bg;

  @override
  void paint(Canvas canvas, Size s) {
    if (v.length < 2) return;
    var lo = v.reduce(math.min);
    var hi = v.reduce(math.max);
    if (band != null) {
      lo = math.min(lo, band!.$1);
      hi = math.max(hi, band!.$2);
    }
    double x(int i) => 3 + i / (v.length - 1) * (s.width - 7);
    double y(double val) => s.height - 4 - (val - lo) / ((hi - lo) == 0 ? 1 : hi - lo) * (s.height - 8);
    if (band != null) {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTRB(0, y(band!.$2), s.width, y(band!.$1)), const Radius.circular(3)),
        Paint()..color = color.withValues(alpha: 0.1),
      );
    }
    final path = Path()..moveTo(x(0), y(v[0]));
    for (var i = 1; i < v.length; i++) {
      path.lineTo(x(i), y(v[i]));
    }
    canvas
      ..drawPath(
        path,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1.8
          ..strokeJoin = StrokeJoin.round
          ..strokeCap = StrokeCap.round
          ..color = color,
      )
      ..drawCircle(Offset(x(v.length - 1), y(v.last)), 3, Paint()..color = color);
  }

  @override
  bool shouldRepaint(_SparkPainter o) => o.v != v || o.color != color;
}

enum ChartKind { bar, line, diverging }

/// Bar, line and diverging charts with ticks on the right, as in the prototype. With [labels], tapping or
/// dragging across it shows a rule, a dot and the value with its label (the prototype's chart scrubbing).
class AtriumChart extends StatefulWidget {
  const AtriumChart({
    required this.points,
    required this.color,
    this.kind = ChartKind.bar,
    this.height = 170,
    this.xLabels = const [],
    this.band,
    this.domain,
    this.ticks,
    this.tickFormat,
    this.average,
    this.shade = const [],
    this.semantics = 'Chart',
    this.labels,
    this.unit = '',
    this.format,
    super.key,
  });

  /// One label per point for the readout (e.g. "Tue 4 Nov", "03:10"); null makes the chart static.
  final List<String>? labels;

  /// Unit after the value in the readout.
  final String unit;

  /// Value text in the readout; integers by default.
  final String Function(double)? format;

  final List<double> points;
  final Color color;
  final ChartKind kind;
  final double height;
  final List<(int, String)> xLabels;
  final (double, double)? band;
  final (double, double)? domain;
  final List<double>? ticks;
  final String Function(double)? tickFormat;
  final double? average;

  /// Index ranges shaded in the sleep colour (e.g. last night on the 24 h chart).
  final List<(int, int)> shade;
  final String semantics;

  @override
  State<AtriumChart> createState() => _AtriumChartState();
}

class _AtriumChartState extends State<AtriumChart> with SingleTickerProviderStateMixin {
  int? _i;
  Timer? _hide;

  /// Draws the chart in when it appears and again when its data changes (e.g. W / M / 6M).
  late final AnimationController _draw = AnimationController(vsync: this, duration: Motion.chart)..forward();

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (Motion.reduced(context)) _draw.value = 1;
  }

  bool get _scrubbable => widget.labels != null && widget.labels!.length == widget.points.length && widget.points.isNotEmpty;

  void _at(double dx, double width) {
    final g = _ChartGeom(widget, Size(width, widget.height));
    final i = (dx / g.step).floor().clamp(0, widget.points.length - 1);
    _hide?.cancel();
    if (i != _i) {
      HapticFeedback.selectionClick();
      setState(() => _i = i);
    }
  }

  /// The readout stays a moment after the finger lifts, so a tap can be read.
  void _release() {
    _hide?.cancel();
    _hide = Timer(const Duration(milliseconds: 2500), () {
      if (mounted) setState(() => _i = null);
    });
  }

  @override
  void didUpdateWidget(AtriumChart old) {
    super.didUpdateWidget(old);
    if (_i != null && _i! >= widget.points.length) _i = null;
    if (!listEquals(old.points, widget.points) && !Motion.reduced(context)) _draw.forward(from: 0);
  }

  @override
  void dispose() {
    _hide?.cancel();
    _draw.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final o = widget;
    return Semantics(
      label: o.semantics,
      child: LayoutBuilder(
        builder: (context, box) {
          final w = box.maxWidth;
          final chart = SizedBox(
            height: o.height,
            width: double.infinity,
            child: AnimatedBuilder(
              animation: _draw,
              builder: (_, _) => CustomPaint(painter: _ChartPainter(o, t, _scrubbable ? _i : null, Motion.ease.transform(_draw.value))),
            ),
          );
          if (!_scrubbable) return chart;
          final i = _i;
          return GestureDetector(
            behavior: HitTestBehavior.opaque,
            onTapDown: (d) => _at(d.localPosition.dx, w),
            onTapUp: (_) => _release(),
            onTapCancel: _release,
            onHorizontalDragStart: (d) => _at(d.localPosition.dx, w),
            onHorizontalDragUpdate: (d) => _at(d.localPosition.dx, w),
            onHorizontalDragEnd: (_) => _release(),
            child: Stack(
              clipBehavior: Clip.none,
              children: [
                chart,
                if (i != null)
                  Positioned(
                    top: -6,
                    left: _ChartGeom(o, Size(w, o.height)).x(i).clamp(70.0, math.max(70.0, w - 70)),
                    child: FractionalTranslation(
                      translation: const Offset(-0.5, 0),
                      child: Container(
                        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 5),
                        decoration: BoxDecoration(color: Color.alphaBlend(t.ink.withValues(alpha: 0.06), t.bg), borderRadius: BorderRadius.circular(8)),
                        child: Text.rich(
                          TextSpan(
                            children: [
                              TextSpan(
                                text: (o.format ?? _plain)(o.points[i]),
                                style: TextStyle(fontSize: 15, fontWeight: FontWeight.w600, color: t.ink),
                              ),
                              if (o.unit.isNotEmpty)
                                TextSpan(
                                  text: ' ${o.unit}',
                                  style: TextStyle(fontSize: 12, color: t.ink),
                                ),
                              TextSpan(
                                text: '  ${o.labels![i]}',
                                style: TextStyle(fontSize: 12, color: t.ink2),
                              ),
                            ],
                          ),
                          maxLines: 1,
                        ),
                      ),
                    ),
                  ),
              ],
            ),
          );
        },
      ),
    );
  }
}

String _plain(double v) => v == v.roundToDouble() ? v.toInt().toString() : v.toStringAsFixed(1);

/// Chart geometry shared by the painter and the touch readout.
class _ChartGeom {
  factory _ChartGeom(AtriumChart o, Size s) {
    const pr = 34.0;
    const top = 18.0;
    const bot = 22.0;
    final pw = s.width - pr;
    final ph = s.height - top - bot;
    var lo = o.points.isEmpty ? 0.0 : o.points.reduce(math.min);
    var hi = o.points.isEmpty ? 1.0 : o.points.reduce(math.max);
    if (o.band != null) {
      lo = math.min(lo, o.band!.$1);
      hi = math.max(hi, o.band!.$2);
    }
    if (o.kind == ChartKind.bar) lo = math.min(0, lo);
    if (o.kind == ChartKind.diverging) {
      final m = [lo.abs(), hi.abs(), 0.6].reduce(math.max);
      lo = -m;
      hi = m;
    }
    if (o.domain != null) (lo, hi) = o.domain!;
    final tk = o.ticks ?? niceTicks(lo, hi);
    lo = math.min(lo, tk.first);
    hi = math.max(hi, tk.last);
    return _ChartGeom._(pw, ph, top, lo, hi, tk, pw / math.max(1, o.points.length));
  }
  const _ChartGeom._(this.pw, this.ph, this.top, this.lo, this.hi, this.ticks, this.step);
  final double pw;
  final double ph;
  final double top;
  final double lo;
  final double hi;
  final List<double> ticks;
  final double step;
  double x(int i) => step * (i + 0.5);
  double y(double v) => top + ph - (v - lo) / ((hi - lo) == 0 ? 1 : hi - lo) * ph;
}

List<double> niceTicks(double min, double max, [int n = 3]) {
  final span = (max - min) == 0 ? 1.0 : max - min;
  final raw = span / n;
  final mag = math.pow(10, (math.log(raw) / math.ln10).floor()).toDouble();
  final step = [1, 2, 2.5, 5, 10].map((s) => s * mag).firstWhere((s) => s >= raw);
  final lo = (min / step).floor() * step;
  final hi = (max / step).ceil() * step;
  return [for (var v = lo; v <= hi + step * 1e-6; v += step) double.parse(v.toStringAsFixed(4))];
}

class _ChartPainter extends CustomPainter {
  _ChartPainter(this.o, this.t, this.scrub, [this.progress = 1]);
  final AtriumChart o;
  final AtriumColors t;

  /// 0..1 draw-in: bars grow left to right, a line is revealed left to right.
  final double progress;

  /// Growth of bar [i] of [n] at this point of the draw-in, staggered across the chart.
  double _grow(int i, int n) => n <= 1 ? progress : ((progress * 1.35) - 0.35 * i / (n - 1)).clamp(0.0, 1.0);

  /// Point under the finger, if any.
  final int? scrub;

  @override
  void paint(Canvas canvas, Size s) {
    if (o.points.isEmpty) return;
    final g = _ChartGeom(o, s);
    final (pw, ph, top, lo, tk, step) = (g.pw, g.ph, g.top, g.lo, g.ticks, g.step);
    final n = o.points.length;
    double x(int i) => g.x(i);
    double y(double v) => g.y(v);
    final label = TextStyle(fontSize: 10.5, color: t.ink3);

    void text(String str, Offset at, TextAlign align) {
      final tp = TextPainter(
        text: TextSpan(text: str, style: label),
        textDirection: TextDirection.ltr,
      )..layout();
      final dx = switch (align) {
        TextAlign.right => at.dx - tp.width,
        TextAlign.center => at.dx - tp.width / 2,
        _ => at.dx,
      };
      tp.paint(canvas, Offset(dx, at.dy - tp.height / 2));
    }

    final grid = Paint()
      ..color = t.hair
      ..strokeWidth = 1;
    for (final v in tk) {
      _dash(canvas, Offset(0, y(v)), Offset(pw, y(v)), grid);
      text(o.tickFormat?.call(v) ?? _fmt(v), Offset(s.width - 2, y(v)), TextAlign.right);
    }
    if (o.band != null) {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTRB(0, y(o.band!.$2), pw, math.max(y(o.band!.$1), y(o.band!.$2) + 1)), const Radius.circular(4)),
        Paint()..color = o.color.withValues(alpha: 0.1),
      );
    }
    for (final (a, b) in o.shade) {
      canvas.drawRect(Rect.fromLTWH(x(a) - step / 2, top, (b - a + 1) * step, ph), Paint()..color = t.ir.withValues(alpha: 0.08));
    }
    switch (o.kind) {
      case ChartKind.bar:
        final bw = math.max(2.0, math.min(22.0, step * 0.62));
        for (var i = 0; i < n; i++) {
          final y0 = y(math.max(lo, 0));
          final yv = y0 + (y(math.max(o.points[i], lo)) - y0) * _grow(i, n);
          canvas.drawRRect(
            RRect.fromRectAndRadius(Rect.fromLTWH(x(i) - bw / 2, yv, bw, math.max(1.5, y0 - yv)), Radius.circular(math.min(4, bw / 2))),
            Paint()..color = o.color.withValues(alpha: i == n - 1 ? 1 : 0.78),
          );
        }
      case ChartKind.diverging:
        final bw = math.max(2.0, math.min(16.0, step * 0.6));
        final y0 = y(0);
        canvas.drawLine(
          Offset(0, y0),
          Offset(pw, y0),
          Paint()
            ..color = t.ink3
            ..strokeWidth = 0.75,
        );
        final limit = o.band?.$2 ?? 0;
        for (var i = 0; i < n; i++) {
          final v = o.points[i];
          final yv = y0 + (y(v) - y0) * _grow(i, n);
          // One colour; the side of the zero line says warmer or cooler.
          final c = o.color;
          canvas.drawRRect(
            RRect.fromRectAndRadius(Rect.fromLTWH(x(i) - bw / 2, math.min(yv, y0), bw, math.max(1.5, (y0 - yv).abs())), Radius.circular(math.min(3, bw / 2))),
            Paint()..color = c.withValues(alpha: v.abs() > limit ? 1 : 0.55),
          );
        }
      case ChartKind.line:
        final path = Path()..moveTo(x(0), y(o.points[0]));
        for (var i = 1; i < n; i++) {
          path.lineTo(x(i), y(o.points[i]));
        }
        final area = Path.from(path)
          ..lineTo(x(n - 1), top + ph)
          ..lineTo(x(0), top + ph)
          ..close();
        // Draw-in: reveal the line left to right; the end dot lands when it is complete.
        canvas
          ..save()
          ..clipRect(Rect.fromLTRB(-8, 0, x(0) + (x(n - 1) - x(0)) * progress + 8, s.height))
          ..drawPath(
            area,
            Paint()..shader = ui.Gradient.linear(Offset(0, top), Offset(0, top + ph), [o.color.withValues(alpha: 0.22), o.color.withValues(alpha: 0)]),
          )
          ..drawPath(
            path,
            Paint()
              ..style = PaintingStyle.stroke
              ..strokeWidth = 2
              ..strokeJoin = StrokeJoin.round
              ..color = o.color,
          )
          ..restore();
        if (progress < 0.98) break;
        canvas
          ..drawCircle(Offset(x(n - 1), y(o.points.last)), 4, Paint()..color = o.color)
          ..drawCircle(
            Offset(x(n - 1), y(o.points.last)),
            4,
            Paint()
              ..style = PaintingStyle.stroke
              ..strokeWidth = 2
              ..color = t.card,
          );
    }
    if (o.average != null) {
      _dash(
        canvas,
        Offset(0, y(o.average!)),
        Offset(pw, y(o.average!)),
        Paint()
          ..color = o.color
          ..strokeWidth = 1,
        dash: 4,
        gap: 3,
      );
    }
    for (final (i, str) in o.xLabels) {
      text(str, Offset(x(i).clamp(8, pw - 8), s.height - 8), TextAlign.center);
    }
    final k = scrub;
    if (k != null && k < n) {
      canvas
        ..drawLine(
          Offset(x(k), top - 4),
          Offset(x(k), top + ph),
          Paint()
            ..color = t.ink2
            ..strokeWidth = 1,
        )
        ..drawCircle(Offset(x(k), y(o.points[k])), 4.5, Paint()..color = o.color)
        ..drawCircle(
          Offset(x(k), y(o.points[k])),
          4.5,
          Paint()
            ..style = PaintingStyle.stroke
            ..strokeWidth = 2
            ..color = t.card,
        );
    }
  }

  String _fmt(double v) => v == v.roundToDouble() ? v.toInt().toString() : v.toString();

  void _dash(Canvas c, Offset a, Offset b, Paint p, {double dash = 2, double gap = 4}) {
    final len = (b - a).distance;
    final dir = (b - a) / len;
    for (var d = 0.0; d < len; d += dash + gap) {
      c.drawLine(a + dir * d, a + dir * math.min(d + dash, len), p);
    }
  }

  @override
  bool shouldRepaint(_ChartPainter old) => true;
}

/// Sleep stages for last night.
class Hypnogram extends StatelessWidget {
  const Hypnogram({required this.blocks, this.height = 150, this.start = '23:08', this.mid = '02:51', this.end = '06:34', super.key});
  final List<StageBlock> blocks;
  final double height;
  final String start;
  final String mid;
  final String end;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: height,
    width: double.infinity,
    child: CustomPaint(painter: _HypPainter(blocks, AtriumColors.of(context), [start, mid, end])),
  );
}

class _HypPainter extends CustomPainter {
  _HypPainter(this.b, this.t, this.times);
  final List<StageBlock> b;
  final AtriumColors t;
  final List<String> times;

  @override
  void paint(Canvas canvas, Size s) {
    const top = 6.0;
    const lab = 44.0;
    final rh = (s.height - top - 24) / 4;
    final pw = s.width - lab;
    final total = b.fold<int>(0, (a, x) => a + x.minutes);
    if (total == 0) return; // no night yet
    double x(num m) => lab + m / total * pw;
    double rowY(SleepStage st) => top + st.index * rh;
    final style = TextStyle(fontSize: 10.5, color: t.ink3);
    const names = ['Awake', 'REM', 'Light', 'Deep'];
    for (final st in SleepStage.values) {
      final tp = TextPainter(
        text: TextSpan(text: names[st.index], style: style),
        textDirection: TextDirection.ltr,
      )..layout();
      tp.paint(canvas, Offset(0, rowY(st) + rh / 2 - tp.height / 2));
      canvas.drawLine(Offset(lab, rowY(st) + rh / 2), Offset(s.width, rowY(st) + rh / 2), Paint()..color = t.hair);
    }
    var tm = 0;
    SleepStage? prev;
    const op = [0.35, 0.6, 0.8, 1.0];
    for (final blk in b) {
      final x0 = x(tm);
      final x1 = x(tm + blk.minutes);
      if (prev != null) {
        canvas.drawLine(
          Offset(x0, rowY(prev) + rh / 2),
          Offset(x0, rowY(blk.stage) + rh / 2),
          Paint()
            ..color = t.ir.withValues(alpha: 0.35)
            ..strokeWidth = 1,
        );
      }
      final c = blk.stage == SleepStage.awake ? t.ink3 : t.ir;
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(x0, rowY(blk.stage) + 3, math.max(1.5, x1 - x0 - 0.5), rh - 6), const Radius.circular(3)),
        Paint()..color = c.withValues(alpha: op[blk.stage.index]),
      );
      prev = blk.stage;
      tm += blk.minutes;
    }
    for (var i = 0; i < 3; i++) {
      final tp = TextPainter(
        text: TextSpan(text: times[i], style: style),
        textDirection: TextDirection.ltr,
      )..layout();
      final m = [0, total / 2, total][i];
      final dx = [x(m), x(m) - tp.width / 2, x(m) - tp.width][i];
      tp.paint(canvas, Offset(dx, s.height - tp.height - 1));
    }
  }

  @override
  bool shouldRepaint(_HypPainter o) => false;
}
