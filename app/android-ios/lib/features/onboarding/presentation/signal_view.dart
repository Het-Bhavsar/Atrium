import 'dart:async';
import 'dart:math' as math;

import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:atrium/features/pod/domain/fit_checks.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 9 (#1, #18, #24): live pulse and Fit Quality, then a 10 s press to seat the pad.
class SignalView extends ConsumerStatefulWidget {
  const SignalView({super.key});

  @override
  ConsumerState<SignalView> createState() => _SignalViewState();
}

enum _Phase { dark, moving, settle, good }

/// Simulated Fit Quality notifications (prototype ob-signal mount): not on skin -> moving -> settling -> fit good.
/// Replaced by the pod's Fit Quality 0x0206 stream later.
({_Phase phase, FitQuality fit}) _simulate(double t) {
  if (t < 1.6) return (phase: _Phase.dark, fit: const FitQuality(q: 0, flags: 0x04));
  if (t < 3.2) return (phase: _Phase.moving, fit: const FitQuality(q: 30, flags: 0x01));
  if (t < 6.5) return (phase: _Phase.settle, fit: FitQuality(q: (100 * (0.45 + (t - 3.2) / 8).clamp(0, 0.82)).round(), flags: 0x10));
  return (phase: _Phase.good, fit: const FitQuality(q: 90, flags: 0x30));
}

/// Real builds: the phase shown for a Fit Quality notification (green only on bit 5).
({_Phase phase, FitQuality fit}) _fromPod(FitQuality f) => !f.measuring
    ? (phase: _Phase.dark, fit: FitQuality(q: 0, flags: f.flags | 0x04))
    : (
        phase: f.dark || f.saturated
            ? _Phase.dark
            : f.moving
            ? _Phase.moving
            : f.fitGood
            ? _Phase.good
            : _Phase.settle,
        fit: f,
      );

class _SignalViewState extends ConsumerState<SignalView> with SingleTickerProviderStateMixin {
  ({_Phase phase, FitQuality fit})? _live;
  int? _bpm;
  final _subs = <StreamSubscription<Object?>>[];
  Timer? _fitRenew;

  ({_Phase phase, FitQuality fit}) _sample(double t) => kRealData ? (_live ?? (phase: _Phase.dark, fit: const FitQuality(q: 0, flags: 0x04))) : _simulate(t);

  late final Ticker _ticker = createTicker(_onTick);
  final _time = ValueNotifier<double>(0);
  Timer? _press;

  _Phase? _phase;
  int _q = 8;
  bool _fitGood = false;

  /// Seconds left while pressing; null before the press starts.
  int? _left;

  /// Fit good, once a second during the press; the press counts when it held for 8 of the 10 s.
  final _pressGood = <bool>[];
  bool _retry = false;

  @override
  void initState() {
    super.initState();
    _ticker.start();
    final link = ref.read(podLinkProvider);
    if (kRealData && link != null) {
      // The pod leaves fit mode by itself after 180 s; renew it while this step is open.
      _fitRenew = Timer.periodic(const Duration(seconds: 150), (_) => link.renewFit());
      _subs
        ..add(link.startFit().listen((f) => _live = _fromPod(f)))
        ..add(
          link.heartRate().listen((b) {
            if (mounted) setState(() => _bpm = b);
          }),
        );
    }
  }

  void _onTick(Duration elapsed) {
    final t = elapsed.inMicroseconds / 1e6;
    _time.value = t;
    final s = _sample(t);
    if (s.phase != _phase || s.fit.q != _q) {
      setState(() {
        _phase = s.phase;
        _q = s.fit.q;
        // Green only on Fit Quality bit 5; once good, the press step stays armed.
        _fitGood = _fitGood || s.fit.fitGood;
      });
    }
  }

  void _startPress() {
    _pressGood.clear();
    setState(() {
      _left = 10;
      _retry = false;
    });
    _press = Timer.periodic(const Duration(seconds: 1), (timer) {
      if (!mounted) return timer.cancel();
      _pressGood.add(_sample(_time.value).fit.fitGood);
      setState(() => _left = _left! - 1);
      if (_left! <= 0) {
        timer.cancel();
        if (!pressOk(_pressGood)) {
          setState(() {
            _left = null;
            _retry = true;
          });
          return;
        }
        _press = Timer(const Duration(milliseconds: 600), () {
          if (mounted) context.push('/baseline');
        });
      }
    });
  }

  @override
  void dispose() {
    _ticker.dispose();
    _press?.cancel();
    _fitRenew?.cancel();
    for (final s in _subs) {
      s.cancel();
    }
    if (kRealData) ref.read(podLinkProvider)?.stopFit();
    _time.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final quality = switch (_phase) {
      null => 'Finding',
      _Phase.dark => 'Not on skin',
      _Phase.moving => 'Hold still',
      _Phase.settle => 'Finding your pulse',
      _Phase.good => 'Good fit',
    };
    final left = _left;
    final label = left == null ? (_retry ? 'Press again' : 'Press for 10 seconds') : (left > 0 ? 'Keep pressing… $left' : 'Fit confirmed');

    return OnboardingScaffold(
      content: [
        const StepsDots(7),
        ObTitle(_fitGood ? 'Good signal' : 'Checking the signal'),
        ObLede(
          _retry
              ? 'The fit slipped during the press. Move the pod a little and press again.'
              : _fitGood
              ? 'Press the pod firmly onto your skin for 10 seconds.'
              : 'Hold still. If the bar stays grey, slide the pod a few millimetres.',
        ),
        Padding(
          padding: const EdgeInsets.only(top: 8, bottom: 18),
          child: AtriumCard(
            child: Column(
              children: [
                Padding(
                  padding: const EdgeInsets.only(bottom: 6),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.baseline,
                    textBaseline: TextBaseline.alphabetic,
                    children: [
                      Expanded(child: Text('Your pulse', style: AtriumType.secondary(t.ink2))),
                      AnimatedOpacity(
                        opacity: _fitGood ? 1 : 0,
                        duration: const Duration(milliseconds: 400),
                        child: BigValue(kRealData ? '${_bpm ?? '\u2013'}' : '62', 'bpm', size: 26),
                      ),
                    ],
                  ),
                ),
                SizedBox(
                  height: 110,
                  width: double.infinity,
                  child: CustomPaint(
                    painter: _PpgPainter(time: _time, color: t.green, hair: t.hair, sample: _sample),
                  ),
                ),
              ],
            ),
          ),
        ),
        AtriumCard(
          child: Column(
            children: [
              Padding(
                padding: const EdgeInsets.only(bottom: 10),
                child: Row(
                  children: [
                    Expanded(
                      child: Text('Signal', style: AtriumType.bodyStrong(t.ink).copyWith(fontWeight: FontWeight.w700)),
                    ),
                    Text(quality, style: AtriumType.body(t.ink2)),
                  ],
                ),
              ),
              Meter(_q / 100, color: _phase == _Phase.good ? t.green : t.ink3),
            ],
          ),
        ),
        const Spacer(),
      ],
      footerGap: 22,
      footer: [
        PrimaryButton(label, onPressed: _fitGood && left == null ? _startPress : null),
        PlainButton(
          'Signal stays weak?',
          onPressed: () => showObSheet(
            context,
            title: 'Getting a clear signal',
            paragraphs: const [
              'Slide the pod a few millimetres toward your eye, or a little lower. It should sit flat on bare skin, upright, window end up.',
              'Dry skin and a fresh pad help. If you wear glasses, keep the arm just under the pod.',
            ],
            button: 'Try again',
          ),
        ),
      ],
    );
  }
}

/// Live PPG trace: two Gaussian pulses per beat at 62 bpm over a 4 s window, with motion noise
/// until the fit is good. Repaints from [time] without rebuilding the page.
class _PpgPainter extends CustomPainter {
  _PpgPainter({required this.time, required this.color, required this.hair, required this.sample}) : super(repaint: time);
  final ({_Phase phase, FitQuality fit}) Function(double t) sample;
  final ValueNotifier<double> time;
  final Color color;
  final Color hair;

  static const double _window = 4;

  static double _g(double x, double m, double s) => math.exp(-((x - m) * (x - m)) / (2 * s * s));

  static double _ppg(double t) {
    final ph = (t * 62 / 60) % 1;
    return _g(ph, 0.16, 0.055) + 0.38 * _g(ph, 0.43, 0.08) + 0.06 * math.sin(t * 1.3);
  }

  @override
  void paint(Canvas canvas, Size size) {
    final t = time.value;
    final w = size.width;
    final h = size.height;
    final base = h * 0.78;

    final dash = Paint()
      ..color = hair
      ..strokeWidth = 0.5;
    for (double x = 0; x < w; x += 4.5) {
      canvas.drawLine(Offset(x, base), Offset(math.min(x + 1.5, w), base), dash);
    }

    final s = sample(t);
    final q = s.fit.q / 100;
    final noise = s.phase == _Phase.good ? 0.0 : (1 - q) * 0.6;
    final path = Path();
    for (double x = 0; x <= w; x += 1.5) {
      final tt = t - _window + x / w * _window;
      final v = _ppg(tt) + noise * math.sin(tt * 23) * math.sin(tt * 7.1);
      final y = base - v * h * 0.62;
      x == 0 ? path.moveTo(x, y) : path.lineTo(x, y);
    }
    canvas.drawPath(
      path,
      Paint()
        ..color = color
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2
        ..strokeJoin = StrokeJoin.round,
    );
  }

  @override
  bool shouldRepaint(_PpgPainter old) => old.color != color || old.hair != hair || old.time != time;
}
