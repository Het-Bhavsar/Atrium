// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/story.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:atrium/features/today/presentation/today_view.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Heart (#1) and HRV (#2, V1): the photo with the pulse on it, one sentence, one range switch, then resting heart
/// rate, HRV and the last 24 hours. Green: the light the pod reads the pulse with.
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
    final pod = ref.watch(podStatusProvider);
    // Real builds: Heart Rate Measurement (0x2A37) while connected and on skin; demo: the prototype's 62 on EVT.
    final live = kRealData
        ? ref.watch(podSessionProvider.select((p) => p.liveBpm))
        : (s.isEvt && pod.connected && !pod.offSkin && !pod.onPlinth ? 62 : null);
    final low = lowestOf(d.hr24);
    return CoverPage(
      image: 'heart',
      title: 'Heart',
      subtitle: live != null ? 'Now' : 'Resting, last night',
      reading: CoverNumber('${live ?? d.last.rhr.round()}', 'bpm'),
      alignment: const Alignment(0, -0.5),
      story: story(
        d,
        demo: 'Resting heart rate was ${d.last.rhr.round()} bpm, a little under your usual ${u.lo.round()}–${u.hi.round()}.',
        real: () => d.last.rhr > 0
            ? 'Resting heart rate was ${d.last.rhr.round()} bpm last night${u.mean > 0 ? ', against a usual ${u.lo.round()}–${u.hi.round()}' : ''}.'
            : 'No resting heart rate for last night yet.',
      ),
      children: [
        RangeSwitch(selected: range, onChanged: (r) => ref.read(appStateProvider.notifier).setRange('heart', r)),
        SectionHeader('Resting heart rate', trailing: u.mean > 0 ? 'Usual ${u.lo.round()}–${u.hi.round()}' : null),
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
          SectionHeader('HRV', trailing: d.last.hrv > 0 ? '${d.last.hrv.round()} ms last night' : null),
          AtriumCard(
            child: AtriumChart(
              points: [for (final p in hrv.points) p.value],
              xLabels: hrv.xLabels,
              color: t.heart,
              kind: range == Range.sixMonths ? ChartKind.line : ChartKind.bar,
              band: (uh.lo, uh.hi),
              labels: [for (final p in hrv.points) p.label],
              unit: 'ms',
              format: (v) => '${v.round()}',
              semantics: 'Heart rate variability',
            ),
          ),
        ],
        SectionHeader('Last 24 hours', trailing: low == null ? null : 'Lowest ${low.bpm} at ${clockLabel(low.minute)}'),
        AtriumCard(child: Hr24Chart(data: d)),
      ],
    );
  }
}

/// The lowest 5-min pulse in [hr] and its time, or null when there is none.
({int bpm, int minute})? lowestOf(List<HrSample> hr) {
  if (hr.isEmpty) return null;
  final m = hr.reduce((a, b) => b.bpm < a.bpm ? b : a);
  return (bpm: m.bpm, minute: m.minute);
}
