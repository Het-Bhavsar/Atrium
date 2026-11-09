import 'package:atrium/core/app_state.dart';
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

/// Skin temperature: nightly change against the usual ±0.3 °C band.
class TemperatureView extends ConsumerWidget {
  const TemperatureView({super.key});

  static String _tick(double v) => v == 0 ? '0' : '${v > 0 ? '+' : '−'}${v.abs()}';

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final d = ref.watch(metricsProvider);
    final t = AtriumColors.of(context);
    final range = s.ranges['temp'] ?? Range.month;
    final r = d.series(Metric.temp, range);
    return CoverPage(
      image: 'temp',
      title: 'Skin temp',
      subtitle: 'Last night vs usual',
      reading: CoverNumber(signed1(d.last.tempDelta), '°C'),
      story: story(
        d,
        demo: 'Within your usual range all month. The last time it ran warm was three nights in July.',
        real: () => d.days.length < 2
            ? 'Atrium needs a few nights to learn your usual skin temperature.'
            : 'Last night was ${signed1(d.last.tempDelta)} °C from your usual.',
      ),
      children: [
        Segmented<Range>(
          values: Range.values,
          labels: [for (final r in Range.values) r.short],
          selected: range,
          onChanged: (r) => ref.read(appStateProvider.notifier).setRange('temp', r),
        ),
        const SizedBox(height: Space.m),
        AtriumCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              AtriumChart(
                points: [for (final p in r.points) (p.value * 100).round() / 100],
                xLabels: r.xLabels,
                color: t.red,
                kind: range == Range.sixMonths ? ChartKind.line : ChartKind.diverging,
                band: (-0.3, 0.3),
                domain: (-0.6, 0.6),
                ticks: const [-0.5, 0, 0.5],
                tickFormat: _tick,
                labels: [for (final p in r.points) p.label],
                unit: '°C',
                format: signed1,
                semantics: 'Nightly skin temperature change',
              ),
              const SizedBox(height: 10),
              Wrap(
                spacing: 14,
                runSpacing: 6,
                children: [
                  _LegendItem(color: t.red, label: 'Warmer'),
                  _LegendItem(color: t.cool, label: 'Cooler'),
                  _LegendItem(color: t.red.withValues(alpha: 0.15), label: 'Usual ±0.3 °C'),
                ],
              ),
            ],
          ),
        ),
        const FootNote(
          'This is the change in skin temperature, not body temperature. A warm room, late meals, alcohol, travel and your cycle can all move it.',
        ),
      ],
    );
  }
}

class _LegendItem extends StatelessWidget {
  const _LegendItem({required this.color, required this.label});
  final Color color;
  final String label;

  @override
  Widget build(BuildContext context) => Row(
    mainAxisSize: MainAxisSize.min,
    children: [
      Container(
        width: 10,
        height: 10,
        decoration: BoxDecoration(color: color, borderRadius: BorderRadius.circular(3)),
      ),
      const SizedBox(width: 5),
      Text(label, style: AtriumType.caption(AtriumColors.of(context).ink2)),
    ],
  );
}
