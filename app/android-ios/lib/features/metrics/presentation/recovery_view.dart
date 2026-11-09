import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/util/format.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/story.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Recovery (balance): what shaped this morning's score, and its trend.
class RecoveryView extends ConsumerWidget {
  const RecoveryView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final d = ref.watch(metricsProvider);
    final t = AtriumColors.of(context);
    final last = d.last;
    final uh = d.usual[Metric.hrv]!;
    final ur = d.usual[Metric.rhr]!;
    final range = s.ranges['trends'] ?? Range.month;
    final r = d.series(Metric.recovery, range);
    // Real builds: no Recovery formula yet, so no verdict words or bars, just the readings.
    final factors = [
      ('HRV', '${last.hrv.round()} ms · usual ${uh.lo.round()}–${uh.hi.round()}', 86, 'Above usual'),
      ('Resting heart rate', '${last.rhr.round()} bpm · usual ${ur.lo.round()}–${ur.hi.round()}', 80, 'Lower'),
      ('Sleep', hm(d.asleep), 74, 'Good'),
      ('Skin temperature', '${signed1(last.tempDelta)} °C from usual', 70, 'Usual'),
      ("Yesterday's effort", d.days.length < 2 ? 'Not measured yet' : 'Effort ${d.days[d.days.length - 2].effort}', 62, 'Moderate'),
    ];
    if (kRealData) {
      for (var i = 0; i < factors.length; i++) {
        factors[i] = (factors[i].$1, factors[i].$2, 0, '');
      }
    }
    return CoverPage(
      image: 'sky-dawn',
      title: 'Recovery',
      subtitle: 'This morning',
      reading: CoverArch(last.recovery.toDouble(), t.balance),
      alignment: const Alignment(0, -0.2),
      story: story(
        d,
        demo: 'Well rested. Your heart was calm overnight and HRV is above your usual.',
        real: () => 'Recovery starts once Atrium knows your usual range. Until then, here are last night\'s readings.',
      ),
      children: [
        const SectionHeader('What shaped it'),
        ListGroup(
          children: [
            for (final (name, sub, v, word) in factors)
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: Space.l, vertical: 14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(name, style: AtriumType.bodyStrong(t.ink)),
                              Padding(
                                padding: const EdgeInsets.only(top: 2),
                                child: Text(sub, style: AtriumType.secondary(t.ink2).copyWith(fontSize: 13.5, height: 1.38)),
                              ),
                            ],
                          ),
                        ),
                        const SizedBox(width: 10),
                        Text(word, style: AtriumType.secondary(t.ink2)),
                      ],
                    ),
                    const SizedBox(height: Space.s),
                    Meter(v / 100, color: t.balance),
                  ],
                ),
              ),
          ],
        ),
        const SectionHeader('Trend'),
        Segmented<Range>(
          values: Range.values,
          labels: [for (final r in Range.values) r.short],
          selected: range,
          onChanged: (r) => ref.read(appStateProvider.notifier).setRange('trends', r),
        ),
        const SizedBox(height: Space.m),
        AtriumCard(
          child: AtriumChart(
            points: [for (final p in r.points) p.value],
            xLabels: r.xLabels,
            color: t.balance,
            kind: range == Range.sixMonths ? ChartKind.line : ChartKind.bar,
            domain: (0, 100),
            ticks: const [0, 50, 100],
            labels: [for (final p in r.points) p.label],
            semantics: 'Recovery trend',
          ),
        ),
        const FootNote('Recovery compares last night with your own usual range. A wellness estimate, not a medical measurement.'),
      ],
    );
  }
}
