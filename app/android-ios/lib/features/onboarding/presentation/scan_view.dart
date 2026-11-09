import 'dart:async';

import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 5 (#15): scanning, then the found pod with a Pair button.
class ScanView extends ConsumerStatefulWidget {
  const ScanView({super.key});

  @override
  ConsumerState<ScanView> createState() => _ScanViewState();
}

class _ScanViewState extends ConsumerState<ScanView> with SingleTickerProviderStateMixin {
  /// Prototype `mount()`: the pod shows up after 2.2 s. Replaced by the BLE scan later.
  static const _foundAfter = Duration(milliseconds: 2200);

  late final AnimationController _rings = AnimationController(vsync: this, duration: const Duration(milliseconds: 2400))..repeat();
  Timer? _timer;
  bool _found = false;

  @override
  void initState() {
    super.initState();
    if (kRealData) {
      WidgetsBinding.instance.addPostFrameCallback((_) => ref.read(podSessionProvider.notifier).scan());
      return;
    }
    _timer = Timer(_foundAfter, () {
      if (mounted) setState(() => _found = true);
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    if (kRealData) ref.read(podSessionProvider.notifier).stopScan();
    _rings.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    // Real builds: the strongest pod the scan found (Power service 0x0100 in its advertising).
    final session = kRealData ? ref.watch(podSessionProvider) : null;
    final pods = [...?session?.found]..sort((a, b) => b.rssi.compareTo(a.rssi));
    final FoundPod? pod = pods.isEmpty ? null : pods.first;
    if (kRealData) _found = pod != null;
    final name = pod?.name ?? 'Atrium 3F2A';
    final signal = pod == null
        ? 'signal strong'
        : pod.rssi > -60
        ? 'signal strong'
        : pod.rssi > -75
        ? 'signal fair'
        : 'move closer';
    return OnboardingScaffold(
      footerGap: 14,
      content: [
        const StepsDots(4),
        ObTitle(_found ? 'Found your pod' : 'Looking for Atrium'),
        ObLede(_found ? 'Tap Pair to connect it to this phone.' : 'Keep the pod close to your phone.'),
        ObArt(child: _ScanOrb(rings: _rings)),
        IgnorePointer(
          ignoring: !_found,
          child: AnimatedSlide(
            offset: _found ? Offset.zero : const Offset(0, 0.35),
            duration: const Duration(milliseconds: 500),
            curve: Motion.ease,
            child: AnimatedOpacity(
              opacity: _found ? 1 : 0,
              duration: const Duration(milliseconds: 400),
              child: AtriumCard(
                child: Row(
                  children: [
                    Container(
                      width: 54,
                      height: 54,
                      clipBehavior: Clip.antiAlias,
                      decoration: BoxDecoration(shape: BoxShape.circle, color: t.track),
                      child: OverflowBox(maxWidth: 65, maxHeight: 65, child: Image.asset('assets/img/pod.png', width: 65)),
                    ),
                    const SizedBox(width: 14),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            name,
                            style: TextStyle(fontSize: 17, fontWeight: FontWeight.w700, color: t.ink),
                          ),
                          Text('EVT pod · $signal', style: AtriumType.secondary(t.ink2)),
                        ],
                      ),
                    ),
                    ObPillButton('Pair', onPressed: () => context.push('/pair', extra: pod)),
                  ],
                ),
              ),
            ),
          ),
        ),
      ],
      footer: [
        PlainButton(
          'Not finding it?',
          onPressed: () => showObSheet(
            context,
            title: 'Not finding your pod?',
            paragraphs: const [
              'Put it back on the Plinth for a moment, then lift it off again. It looks for your phone for one minute after each lift.',
              'If nothing happens, the battery may be empty. Leave it on the Plinth for 15 minutes and try again.',
            ],
          ),
        ),
      ],
    );
  }
}

/// `.scan-orb`: three green rings swelling out of the pod disc, 0.8 s apart.
class _ScanOrb extends StatelessWidget {
  const _ScanOrb({required this.rings});
  final Animation<double> rings;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return SizedBox(
      width: 220,
      height: 220,
      child: Stack(
        alignment: Alignment.center,
        clipBehavior: Clip.none,
        children: [
          for (var i = 0; i < 3; i++)
            AnimatedBuilder(
              animation: rings,
              builder: (_, _) {
                final p = Motion.ease.transform((rings.value + i / 3) % 1);
                return Opacity(
                  opacity: 0.55 * (1 - p),
                  child: Transform.scale(
                    scale: 0.6 + 2 * p,
                    child: Container(
                      width: 150,
                      height: 150,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        border: Border.all(color: t.green, width: 1.5),
                      ),
                    ),
                  ),
                );
              },
            ),
          Container(
            width: 150,
            height: 150,
            clipBehavior: Clip.antiAlias,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: t.cardSolid,
              border: Border.all(color: t.hair, width: 0.5),
              boxShadow: const [BoxShadow(color: Color(0x59000000), blurRadius: 50, offset: Offset(0, 20))],
            ),
            child: Padding(
              padding: const EdgeInsets.all(22),
              child: Image.asset('assets/img/pod.png', fit: BoxFit.contain),
            ),
          ),
        ],
      ),
    );
  }
}
