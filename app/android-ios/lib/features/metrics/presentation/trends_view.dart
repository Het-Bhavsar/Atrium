import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/routes/app_router.dart';
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

/// Trends tab (#7): averages over the chosen range, each with a sparkline.
class TrendsView extends ConsumerWidget {
  const TrendsView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final d = ref.watch(metricsProvider);
    final t = AtriumColors.of(context);

    if (s.data == DataState.empty) {
      return PlainPage(
        title: 'Trends',
        children: [
          AtriumCard(
            padding: const EdgeInsets.symmetric(horizontal: 22, vertical: 40),
            child: Column(
              children: [
                AIcon('trends', size: 42, color: t.ink3, stroke: 1.4),
                const SizedBox(height: Space.m),
                Text(
                  'Trends need a few days',
                  textAlign: TextAlign.center,
                  style: AtriumType.serifAt(26, color: t.ink),
                ),
                const SizedBox(height: Space.s),
                Text("After a week of wear you'll see how your nights and days are changing.", textAlign: TextAlign.center, style: AtriumType.body(t.ink2)),
              ],
            ),
          ),
        ],
      );
    }

    final range = s.ranges['trends'] ?? Range.month;
    final k = switch (range) {
      Range.week => 7,
      Range.month => 30,
      Range.sixMonths => d.days.length,
    };
    final sl = d.days.sublist((d.days.length - k).clamp(0, d.days.length));
    double mean(Metric m) => sl.isEmpty ? 0 : sl.map((x) => x.value(m)).fold(0.0, (a, b) => a + b) / sl.length;
    List<double> spark(Metric m) => [for (final p in d.series(m, range).points) p.value];
    final ur = d.usual[Metric.rhr]!;
    final uh = d.usual[Metric.hrv]!;
    final evt = s.isEvt;

    _TrendRow row(String icon, Color color, String name, String value, String route, Metric m, [(double, double)? band]) =>
        _TrendRow(icon: icon, color: color, name: name, value: value, values: spark(m), band: band, onTap: () => context.pushDetail(route));

    return PlainPage(
      title: 'Trends',
      children: [
        Segmented<Range>(
          values: Range.values,
          labels: [for (final r in Range.values) r.long],
          selected: range,
          onChanged: (r) => ref.read(appStateProvider.notifier).setRange('trends', r),
        ),
        if (!evt)
          Padding(
            padding: const EdgeInsets.fromLTRB(4, 16, 4, 6),
            child: StoryText(
              story(
                d,
                demo: 'HRV has been above your usual for six nights, after a steady month.',
                real: () => 'Averages from the ${d.days.length} days synced so far.',
              ),
            ),
          ),
        SectionHeader(
          'Averages, ${switch (range) {
            Range.week => 'last 7 days',
            Range.month => 'last 30 days',
            Range.sixMonths => 'last 6 months',
          }}',
        ),
        ListGroup(
          children: [
            if (!evt) row('balance', t.balance, 'Recovery', '${mean(Metric.recovery).round()}', '/recovery', Metric.recovery),
            if (!evt) row('moon', t.sleep, 'Time asleep', hm(mean(Metric.sleepMin)), '/sleep', Metric.sleepMin),
            if (!evt) row('effort', t.effort, 'Effort', '${mean(Metric.effort).round()}', '/effort', Metric.effort),
            row('heart', t.heart, 'Resting heart rate', '${mean(Metric.rhr).round()} bpm', '/heart', Metric.rhr, (ur.lo, ur.hi)),
            if (!evt) row('hrv', t.green, 'HRV', '${mean(Metric.hrv).round()} ms', '/heart', Metric.hrv, (uh.lo, uh.hi)),
            row('temp', t.red, 'Skin temperature', '${signed1(mean(Metric.temp))} °C from usual', '/temperature', Metric.temp, (-0.3, 0.3)),
            if (!evt) row('effort', t.effort, 'Active minutes', '${mean(Metric.active).round()} a day', '/effort', Metric.active),
          ],
        ),
        Padding(
          padding: const EdgeInsets.fromLTRB(Space.l, 9, Space.l, 0),
          child: Text(
            'Shaded bands show your usual range over the previous 30 days.',
            style: AtriumType.secondary(t.ink2).copyWith(fontSize: 13.5, height: 1.45),
          ),
        ),
      ],
    );
  }
}

class _TrendRow extends StatelessWidget {
  const _TrendRow({required this.icon, required this.color, required this.name, required this.value, required this.values, required this.onTap, this.band});
  final String icon;
  final Color color;
  final String name;
  final String value;
  final List<double> values;
  final (double, double)? band;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return InkWell(
      onTap: onTap,
      child: ConstrainedBox(
        constraints: const BoxConstraints(minHeight: 48),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: Space.l, vertical: 10),
          child: Row(
            children: [
              SizedBox(width: 22, child: AIcon(icon, color: color)),
              const SizedBox(width: 14),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(name, style: AtriumType.bodyStrong(t.ink)),
                    Padding(
                      padding: const EdgeInsets.only(top: 2),
                      child: Text(value, style: AtriumType.secondary(t.ink2).copyWith(fontSize: 13.5, height: 1.38)),
                    ),
                  ],
                ),
              ),
              const SizedBox(width: 14),
              SizedBox(
                width: 96,
                child: Sparkline(values: values, color: color, height: 28, band: band),
              ),
              const SizedBox(width: 14),
              AIcon('chev', size: 14, color: t.ink3, stroke: 2.2),
            ],
          ),
        ),
      ),
    );
  }
}
