// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/story.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Effort (activity): today's workouts, heart rate zones, and effort by day.
class EffortView extends ConsumerWidget {
  const EffortView({super.key});

  static const _zones = [('Easy', 182, 0.35), ('Steady', 64, 0.55), ('Brisk', 31, 0.75), ('Hard', 14, 0.9), ('Peak', 4, 1.0)];
  static const _workouts = [('Run', '07:10', 38, 134), ('Walk', '13:00', 30, 96)];

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final d = ref.watch(metricsProvider);
    final t = AtriumColors.of(context);
    final range = s.ranges['effort'] ?? Range.week;
    final r = d.series(Metric.effort, range);
    final zmax = _zones.map((z) => z.$2).reduce((a, b) => a > b ? a : b);
    return CoverPage(
      image: 'effort',
      title: 'Effort',
      subtitle: '${d.last.active} min active today',
      reading: CoverArch(d.last.effort.toDouble(), t.effort),
      story: story(
        d,
        demo: 'A steady day. The morning run did most of the work; a typical day for you is about ${d.usual[Metric.effort]!.mean.round()}.',
        real: () => '${d.last.active} active minutes and ${d.last.steps} steps today.',
      ),
      children: [
        const SectionHeader('Today'),
        ListGroup(
          children: [for (final (name, time, min, hr) in _workouts) ListRow(icon: 'effort', title: name, subtitle: '$time · $min min · average $hr bpm')],
        ),
        const SectionHeader('Heart rate zones', trailing: 'Minutes today'),
        AtriumCard(
          child: Column(
            children: [
              for (var i = 0; i < _zones.length; i++)
                Padding(
                  padding: EdgeInsets.only(top: i == 0 ? 0 : 10),
                  child: Row(
                    children: [
                      SizedBox(width: 58, child: Text(_zones[i].$1, style: AtriumType.secondary(t.ink))),
                      const SizedBox(width: 10),
                      Expanded(
                        child: ClipRRect(
                          borderRadius: BorderRadius.circular(5),
                          child: Container(
                            height: 10,
                            color: t.track,
                            alignment: Alignment.centerLeft,
                            child: FractionallySizedBox(
                              widthFactor: _zones[i].$2 / zmax,
                              heightFactor: 1,
                              child: DecoratedBox(
                                decoration: BoxDecoration(
                                  color: t.effort.withValues(alpha: _zones[i].$3),
                                  borderRadius: BorderRadius.circular(5),
                                ),
                              ),
                            ),
                          ),
                        ),
                      ),
                      const SizedBox(width: 10),
                      SizedBox(
                        width: 62,
                        child: Text('${_zones[i].$2} min', textAlign: TextAlign.right, style: AtriumType.secondary(t.ink2)),
                      ),
                    ],
                  ),
                ),
            ],
          ),
        ),
        SectionHeader('By day', trailing: 'Average ${r.mean.round()}'),
        Segmented<Range>(
          values: Range.values,
          labels: [for (final r in Range.values) r.short],
          selected: range,
          onChanged: (r) => ref.read(appStateProvider.notifier).setRange('effort', r),
        ),
        const SizedBox(height: Space.m),
        AtriumCard(
          child: AtriumChart(
            points: [for (final p in r.points) p.value],
            xLabels: r.xLabels,
            color: t.effort,
            kind: range == Range.sixMonths ? ChartKind.line : ChartKind.bar,
            domain: (0, 100),
            ticks: const [0, 50, 100],
            labels: [for (final p in r.points) p.label],
            semantics: 'Daily effort',
          ),
        ),
        const FootNote('Effort weights the minutes your heart spent in each zone. Zones come from your age and resting heart rate.'),
      ],
    );
  }
}
