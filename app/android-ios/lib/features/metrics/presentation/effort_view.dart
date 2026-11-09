// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/scores.dart';
import 'package:atrium/features/insights/domain/story.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Effort (#6): the photo with today's score on it, one sentence, one range switch, then effort by day, the minutes in
/// each heart-rate zone and today's walks and runs (Atrium's formula, `scores.dart`). Red: the effort colour.
class EffortView extends ConsumerWidget {
  const EffortView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final d = ref.watch(metricsProvider);
    final t = AtriumColors.of(context);
    final range = s.ranges['effort'] ?? Range.week;
    final r = d.series(Metric.effort, range);
    final zones = d.zoneMinutes;
    final zmax = zones.isEmpty ? 0.0 : zones.reduce((a, b) => a > b ? a : b);
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
        RangeSwitch(selected: range, onChanged: (r) => ref.read(appStateProvider.notifier).setRange('effort', r)),
        SectionHeader('By day', trailing: r.mean > 0 ? 'Average ${r.mean.round()}' : null),
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
        if (zmax > 0) ...[
          const SectionHeader('Heart rate zones', trailing: 'Minutes today'),
          AtriumCard(
            child: Column(
              children: [
                for (var i = 0; i < zones.length && i < effortZones.length; i++)
                  Padding(
                    padding: EdgeInsets.only(top: i == 0 ? 0 : 12),
                    child: Row(
                      children: [
                        SizedBox(width: 62, child: Text(effortZones[i].$1, style: AtriumType.secondary(t.ink))),
                        const SizedBox(width: 10),
                        Expanded(child: Meter(zones[i] / zmax, color: t.effort.withValues(alpha: 0.35 + 0.65 * (i + 1) / effortZones.length), height: 8)),
                        const SizedBox(width: 10),
                        SizedBox(
                          width: 64,
                          child: Text('${zones[i].round()} min', textAlign: TextAlign.right, style: AtriumType.secondary(t.ink2)),
                        ),
                      ],
                    ),
                  ),
              ],
            ),
          ),
        ],
        if (d.workouts.isNotEmpty) ...[
          const SectionHeader('Walks and runs'),
          ReadingList([
            for (final w in d.workouts)
              Reading(w.kind, w.avgBpm > 0 ? '${w.avgBpm}' : '–', unit: 'bpm', note: '${clockLabel(w.startMinute)} · ${w.minutes} min'),
          ]),
        ],
      ],
    );
  }
}
