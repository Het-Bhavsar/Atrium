// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/util/format.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/scores.dart';
import 'package:atrium/features/insights/domain/story.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Recovery (#7): the photo with this morning's score on it, one sentence, one range switch, then the trend and what
/// it was made of (Atrium's formula, `scores.dart`: HRV and resting heart rate against your usual, sleep, skin
/// temperature). Green: the light the pod reads the pulse with.
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
    final range = s.ranges['recovery'] ?? Range.month;
    final r = d.series(Metric.recovery, range);
    final parts = d.recoveryParts;
    return CoverPage(
      image: 'welcome',
      title: 'Recovery',
      subtitle: 'This morning',
      reading: CoverArch(last.recovery.toDouble(), t.balance, learning: parts == null),
      alignment: const Alignment(-0.3, 0.1),
      story: story(d, demo: 'Well rested. Your heart was calm overnight and HRV is above your usual.', real: () => recoveryStory(parts)),
      children: [
        RangeSwitch(selected: range, onChanged: (r) => ref.read(appStateProvider.notifier).setRange('recovery', r)),
        SectionHeader('Trend', trailing: r.mean > 0 ? 'Average ${r.mean.round()}' : null),
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
        const SectionHeader('What shaped it'),
        ReadingList([
          Reading(
            'HRV',
            last.hrv > 0 ? '${last.hrv.round()}' : '–',
            unit: 'ms',
            note: uh.mean > 0 ? 'usual ${uh.lo.round()}–${uh.hi.round()}' : null,
            color: t.balance,
            fraction: parts?.hrv == null ? null : parts!.hrv! / 100,
          ),
          Reading(
            'Resting heart rate',
            last.rhr > 0 ? '${last.rhr.round()}' : '–',
            unit: 'bpm',
            note: ur.mean > 0 ? 'usual ${ur.lo.round()}–${ur.hi.round()}' : null,
            color: t.balance,
            fraction: parts?.rhr == null ? null : parts!.rhr! / 100,
          ),
          Reading('Sleep', d.asleep > 0 ? hm(d.asleep) : '–', color: t.sleep, fraction: parts?.sleep == null ? null : parts!.sleep! / 100),
          Reading(
            'Skin temperature',
            parts?.temp == null && kRealData ? '–' : signed1(last.tempDelta),
            unit: '°C',
            note: 'from your usual',
            fraction: parts?.temp == null ? null : parts!.temp! / 100,
          ),
        ]),
      ],
    );
  }
}

/// The sentence for a real Recovery: the part that lifted it most and, when one was low, what held it back.
String recoveryStory(RecoveryParts? p) {
  if (p == null || p.score == null) return 'Recovery starts after five nights, once Atrium knows your usual.';
  final named = <(String, double)>[
    if (p.hrv != null) ('your HRV', p.hrv!),
    if (p.rhr != null) ('a calm resting heart rate', p.rhr!),
    if (p.sleep != null) ('your sleep', p.sleep!),
    if (p.temp != null) ('a steady skin temperature', p.temp!),
  ]..sort((a, b) => b.$2.compareTo(a.$2));
  final best = named.first;
  final worst = named.last;
  final lift = best.$2 >= 55 ? ' ${_cap(best.$1)} did the most for it.' : '';
  final hold = worst.$2 < 45 && worst != best ? ' ${_cap(worst.$1.replaceFirst('a calm ', '').replaceFirst('a steady ', ''))} held it back.' : '';
  return 'Recovery is ${p.score} this morning.$lift$hold';
}

String _cap(String s) => s.isEmpty ? s : s[0].toUpperCase() + s.substring(1);
