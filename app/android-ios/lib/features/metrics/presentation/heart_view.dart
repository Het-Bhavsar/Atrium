import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/story.dart';
import 'package:atrium/features/today/presentation/today_view.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Heart (#1) and HRV (#2, V1).
class HeartView extends ConsumerWidget {
  const HeartView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final d = ref.watch(metricsProvider);
    final t = AtriumColors.of(context);
    final range = s.ranges['heart'] ?? Range.week;
    final rhr = d.series(Metric.rhr, range);
    final hrv = d.series(Metric.hrv, range);
    final u = d.usual[Metric.rhr]!;
    final uh = d.usual[Metric.hrv]!;
    final live = s.isEvt && s.pod.connected && !s.pod.offSkin && !s.pod.onPlinth;
    return CoverPage(
      image: 'heart',
      title: 'Heart',
      subtitle: live ? 'Now' : 'Resting, last night',
      reading: CoverNumber('${live ? 62 : d.last.rhr.round()}', 'bpm'),
      alignment: const Alignment(0, -0.5),
      story: story(
        d,
        demo: 'Resting heart rate was ${d.last.rhr.round()} bpm, a little under your usual ${u.lo.round()}–${u.hi.round()}.',
        real: () => d.last.rhr > 0 ? 'Resting heart rate was ${d.last.rhr.round()} bpm last night.' : 'No resting heart rate for last night yet.',
      ),
      children: [
        const SectionHeader('Last 24 hours', trailing: 'Lowest 49 at 03:10'),
        AtriumCard(child: Hr24Chart(data: d)),
        const SectionHeader('Resting heart rate', trailing: 'Shaded: your usual'),
        Segmented<Range>(
          values: Range.values,
          labels: [for (final r in Range.values) r.short],
          selected: range,
          onChanged: (r) => ref.read(appStateProvider.notifier).setRange('heart', r),
        ),
        const SizedBox(height: Space.m),
        AtriumCard(
          child: AtriumChart(
            points: [for (final p in rhr.points) p.value],
            xLabels: rhr.xLabels,
            color: t.heart,
            kind: ChartKind.line,
            band: (u.lo, u.hi),
            domain: (45, 66),
            labels: [for (final p in rhr.points) p.label],
            unit: 'bpm',
            format: (v) => '${v.round()}',
            semantics: 'Resting heart rate',
          ),
        ),
        if (!s.isEvt) ...[
          SectionHeader('HRV', trailing: '${d.last.hrv.round()} ms last night'),
          AtriumCard(
            child: AtriumChart(
              points: [for (final p in hrv.points) p.value],
              xLabels: hrv.xLabels,
              color: t.green,
              kind: range == Range.sixMonths ? ChartKind.line : ChartKind.bar,
              band: (uh.lo, uh.hi),
              labels: [for (final p in hrv.points) p.label],
              unit: 'ms',
              format: (v) => '${v.round()}',
              semantics: 'Heart rate variability',
            ),
          ),
        ],
        const FootNote('Green light from one small optical module, read at your temporal region. Samples taken while you move are dropped.'),
      ],
    );
  }
}
