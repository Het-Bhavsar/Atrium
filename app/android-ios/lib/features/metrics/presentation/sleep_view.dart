// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/util/format.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/story.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Sleep (#4, #5): stages, overnight readings, head position, time asleep.
class SleepView extends ConsumerWidget {
  const SleepView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final d = ref.watch(metricsProvider);
    final t = AtriumColors.of(context);
    final learning = s.data == DataState.learning;
    final last = d.last;
    final range = s.ranges['sleep'] ?? Range.week;
    final r = d.series(Metric.sleepMin, range);
    const stages = [('Deep', SleepStage.deep, 1.0), ('REM', SleepStage.rem, 0.6), ('Light', SleepStage.light, 0.8), ('Awake', SleepStage.awake, 0.35)];

    return CoverPage(
      image: 'sleep',
      title: 'Sleep',
      subtitle: '${hm(d.asleep)} asleep',
      reading: CoverArch(last.sleepScore.toDouble(), t.sleep, learning: learning),
      alignment: const Alignment(0, -0.1),
      story: story(d, demo: 'A solid night. Deep sleep came early and you woke once, briefly, just before five.', real: () => sleepStory(d)),
      children: [
        const SectionHeader('Stages', trailing: 'Estimated'),
        AtriumCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Hypnogram(blocks: d.hypnogram, start: clockLabel(d.bedMinute), mid: clockLabel(d.bedMinute + d.inBed ~/ 2), end: clockLabel(d.wakeMinute)),
              const SizedBox(height: 14),
              for (var i = 0; i < stages.length; i += 2)
                Padding(
                  padding: EdgeInsets.only(top: i == 0 ? 0 : 10),
                  child: Row(
                    children: [
                      for (final (name, stage, op) in [stages[i], stages[i + 1]])
                        Expanded(
                          child: _StageCell(
                            name: name,
                            swatch: (stage == SleepStage.awake ? t.ink3 : t.ir).withValues(alpha: op),
                            minutes: d.stageMinutes(stage),
                            percent: d.inBed == 0 ? 0 : (d.stageMinutes(stage) / d.inBed * 100).round(),
                          ),
                        ),
                    ],
                  ),
                ),
            ],
          ),
        ),
        const SectionHeader('Overnight'),
        _TileGrid(
          tiles: [
            MetricTile(icon: 'heart', name: 'Lowest HR', color: t.heart, value: '49', unit: 'bpm', note: 'at 03:10'),
            MetricTile(
              icon: 'hrv',
              name: 'HRV',
              color: t.green,
              value: '${last.hrv.round()}',
              unit: 'ms',
              note: learning ? 'Overnight average' : 'Above usual',
            ),
            MetricTile(icon: 'temp', name: 'Skin temp', color: t.red, value: signed1(last.tempDelta), unit: '°C', note: 'vs usual'),
            MetricTile(icon: 'wave', name: 'Restless', color: t.ink2, value: 'Low', unit: '', note: '11 moves an hour'),
          ],
        ),
        const SectionHeader('Head position', trailing: 'While asleep'),
        _HeadPositionCard(side: s.side, asleep: d.asleep, positions: d.headPositions),
        SectionHeader('Time asleep', trailing: 'Average ${hm(r.mean)}'),
        Segmented<Range>(
          values: Range.values,
          labels: [for (final r in Range.values) r.short],
          selected: range,
          onChanged: (r) => ref.read(appStateProvider.notifier).setRange('sleep', r),
        ),
        const SizedBox(height: Space.m),
        AtriumCard(
          child: AtriumChart(
            points: [for (final p in r.points) p.value / 60],
            xLabels: r.xLabels,
            color: t.sleep,
            kind: range == Range.sixMonths ? ChartKind.line : ChartKind.bar,
            average: 7,
            domain: (0, 9),
            ticks: const [0, 3, 6, 9],
            tickFormat: (v) => '${v.round()} h',
            labels: [for (final p in r.points) p.label],
            format: (v) => hm(v * 60),
            semantics: 'Time asleep',
          ),
        ),
        FootNote(
          d.hasStages
              ? 'Stages are estimated from pulse, HRV and movement: a wellness guide, not a sleep study.'
              : 'Asleep and awake are estimated from movement and pulse by a model still being checked on the pod. A wellness guide, not a sleep study.',
        ),
      ],
    );
  }
}

class _Swatch extends StatelessWidget {
  const _Swatch(this.color);
  final Color color;

  @override
  Widget build(BuildContext context) => Container(
    width: 9,
    height: 9,
    margin: const EdgeInsets.only(right: 6),
    decoration: BoxDecoration(color: color, borderRadius: BorderRadius.circular(3)),
  );
}

class _StageCell extends StatelessWidget {
  const _StageCell({required this.name, required this.swatch, required this.minutes, required this.percent});
  final String name;
  final Color swatch;
  final int minutes;
  final int percent;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            _Swatch(swatch),
            Text(name, style: AtriumType.secondary(t.ink2)),
          ],
        ),
        Text.rich(
          TextSpan(
            children: [
              TextSpan(text: hm(minutes), style: AtriumType.bodyStrong(t.ink)),
              TextSpan(text: ' $percent%', style: AtriumType.secondary(t.ink2)),
            ],
          ),
        ),
      ],
    );
  }
}

/// Minutes on the back and each side while asleep; the pod side reads less well.
class _HeadPositionCard extends StatelessWidget {
  const _HeadPositionCard({required this.side, required this.asleep, required this.positions});
  final WearSide side;
  final int asleep;
  final List<HeadPosition> positions;

  static String _name(HeadSide h) => switch (h) {
    HeadSide.back => 'Back',
    HeadSide.left => 'Left side',
    HeadSide.right => 'Right side',
    HeadSide.front => 'Front',
  };

  static const _opacity = [1.0, 0.7, 0.45];

  static bool _isPodSide(HeadSide h, WearSide side) => (h == HeadSide.left && side == WearSide.left) || (h == HeadSide.right && side == WearSide.right);

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final pos = positions;
    final onPod = pos.where((p) => _isPodSide(p.side, side)).fold(0, (a, p) => a + p.minutes);
    final hint = asleep > 0 && onPod / asleep > 0.25 ? ' For your next pad, try your ${side.other.word} temporal region.' : '';
    return AtriumCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          ClipRRect(
            borderRadius: BorderRadius.circular(6),
            child: SizedBox(
              height: 12,
              child: Row(
                // Stretch: a childless ColoredBox takes the smallest height it is allowed, so loose rows draw nothing.
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  for (var i = 0; i < pos.length; i++) ...[
                    if (i > 0) const SizedBox(width: 2),
                    Expanded(
                      flex: pos[i].minutes,
                      child: ColoredBox(color: t.ir.withValues(alpha: _opacity[i])),
                    ),
                  ],
                ],
              ),
            ),
          ),
          const SizedBox(height: 14),
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              for (var i = 0; i < pos.length; i++) ...[
                if (i > 0) const SizedBox(width: Space.s),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        children: [
                          _Swatch(t.ir.withValues(alpha: _opacity[i])),
                          Flexible(child: Text(_name(pos[i].side), style: AtriumType.secondary(t.ink2))),
                        ],
                      ),
                      Text(hmCompact(pos[i].minutes), style: AtriumType.bodyStrong(t.ink)),
                      if (_isPodSide(pos[i].side, side))
                        Padding(
                          padding: const EdgeInsets.only(top: 2),
                          child: Text('Pod side', style: AtriumType.secondary(t.ir)),
                        ),
                    ],
                  ),
                ),
              ],
            ],
          ),
          const SizedBox(height: Space.l),
          Container(
            padding: const EdgeInsets.only(top: 14),
            decoration: BoxDecoration(
              border: Border(top: BorderSide(color: t.hair, width: 0.5)),
            ),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                SizedBox(width: 22, child: AIcon('device', color: t.ir)),
                const SizedBox(width: Space.m),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('${hm(onPod)} lying on the pod', style: AtriumType.secondary(t.ink).copyWith(fontWeight: FontWeight.w600)),
                      Padding(
                        padding: const EdgeInsets.only(top: 2),
                        child: Text(
                          'Lying on your ${side.word} side presses the pod into the pillow, so those minutes read less well.$hint',
                          style: AtriumType.secondary(t.ink2),
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _TileGrid extends StatelessWidget {
  const _TileGrid({required this.tiles});
  final List<Widget> tiles;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      for (var i = 0; i < tiles.length; i += 2)
        Padding(
          padding: EdgeInsets.only(bottom: i + 2 < tiles.length ? Space.m : 0),
          child: IntrinsicHeight(
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Expanded(child: tiles[i]),
                const SizedBox(width: Space.m),
                Expanded(child: i + 1 < tiles.length ? tiles[i + 1] : const SizedBox()),
              ],
            ),
          ),
        ),
    ],
  );
}
