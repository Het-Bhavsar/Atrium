// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/routes/app_router.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/util/format.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:atrium/features/pod/domain/sync_age.dart';
import 'package:atrium/features/pod/presentation/sync_status.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Today (#1, #3, #13): open to the sky. The sky follows the time of day.
class TodayView extends ConsumerWidget {
  const TodayView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(appStateProvider);
    final d = ref.watch(metricsProvider);
    final t = AtriumColors.of(context);
    final pod = s.pod;
    final learning = s.data == DataState.learning;
    final empty = s.data == DataState.empty;
    final sky = empty ? Sky.night : s.sky;
    final last = d.last;
    final top = MediaQuery.paddingOf(context).top;
    // Real builds: the age of the last sync, and a warning before the pod's log starts overwriting (#20).
    final lastSync = kRealData ? ref.watch(lastSyncProvider) : null;
    final now = DateTime.now();
    final overdue = kRealData && syncOverdue(lastSync, now);
    final syncedLine = kRealData
        ? Padding(
            padding: const EdgeInsets.only(bottom: Space.m),
            child: LastSyncedLine(lastSync: lastSync, now: now, center: true),
          )
        : null;

    final story = learning
        ? 'Your first Recovery score arrives after the third night.'
        : s.sky == Sky.night
        ? "Wind down. Tonight's sleep sets tomorrow's Recovery."
        : "You're well rested. HRV is above your usual and your resting heart rate is low.";
    final evtStory = pod.offSkin
        ? 'Put Atrium back on to keep measuring.'
        : 'Resting heart rate was ${last.rhr.round()} bpm last night, lower than your usual.';
    final live = pod.connected && !pod.offSkin && !pod.onPlinth;

    Widget gauge;
    if (empty) {
      gauge = Column(
        children: [
          const SizedBox(width: 170, child: ArchGauge(value: 0, color: Colors.transparent, learning: true, onPhoto: true)),
          const SizedBox(height: 6),
          Text('No readings yet', style: AtriumType.serifAt(21, color: AtriumColors.onPhoto)),
          const SizedBox(height: 8),
          SizedBox(
            width: 270,
            child: Text(
              'Wear Atrium tonight. Your sleep and overnight heart readings appear here in the morning.',
              textAlign: TextAlign.center,
              style: TextStyle(fontSize: 13, color: AtriumColors.onPhoto.withValues(alpha: 0.72)),
            ),
          ),
          const SizedBox(height: 16),
          SizedBox(width: 150, child: PrimaryButton('How to wear it', onPhoto: true, onPressed: () => context.push('/fit'))),
        ],
      );
    } else if (s.isEvt) {
      gauge = GestureDetector(
        onTap: () => context.pushDetail('/heart'),
        child: SizedBox(
          width: 214,
          child: ArchGauge(
            value: 0,
            color: t.heart,
            noFill: true,
            onPhoto: true,
            number: '${live ? 62 : last.rhr.round()}',
            inner: live ? 'BPM · LIVE' : 'RESTING BPM',
            label: 'Heart rate',
          ),
        ),
      );
    } else {
      gauge = GestureDetector(
        onTap: () => context.pushDetail('/recovery'),
        child: SizedBox(
          width: 214,
          child: ArchGauge(
            value: last.recovery.toDouble(),
            color: t.balance,
            learning: learning,
            onPhoto: true,
            inner: learning ? 'LEARNING' : 'RECOVERY',
            label: 'Recovery',
          ),
        ),
      );
    }

    final hero = SizedBox(
      height: (empty ? 560 : 430) + top,
      child: Stack(
        fit: StackFit.expand,
        children: [
          Image.asset('assets/img/sky-${sky.name}.jpg', fit: BoxFit.cover, alignment: const Alignment(0, -0.6)),
          DecoratedBox(
            decoration: BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                stops: empty ? const [0, 0.36, 0.66, 0.84, 1] : const [0, 0.24, 0.6, 1],
                colors: empty
                    ? [const Color(0x8008080A), const Color(0x3308080A), const Color(0x9908080A), Color.lerp(t.bg, const Color(0x9908080A), 0.45)!, t.bg]
                    : [const Color(0x8008080A), const Color(0x0F08080A), const Color(0x0F08080A), t.bg],
              ),
            ),
          ),
          Padding(
            padding: EdgeInsets.fromLTRB(Space.gutter, top + 44, Space.gutter, 0),
            child: Column(
              children: [
                LargeTitle(
                  'Today',
                  eyebrow: '${_weekday(d.today)} ${d.today.day} ${_month(d.today)}',
                  color: AtriumColors.onPhoto,
                  trailing: PodChip(pod, onPhoto: true),
                ),
                const SizedBox(height: 10),
                gauge,
              ],
            ),
          ),
        ],
      ),
    );

    final notices = <Widget>[
      if (overdue) SyncDueNotice(lastSync: lastSync!, now: now, onTap: () => context.go('/device')),
      if (!pod.connected)
        NoticeCard(
          icon: 'bt',
          title: "Atrium isn't connected",
          body: kRealData
              ? '${lastSyncedSentence(lastSync, now)}. Showing the readings saved on this phone. Tap to reconnect.'
              : pod.lastSync == 'not yet'
              ? 'Showing the readings saved on this phone. Tap to reconnect.'
              : 'Showing readings up to ${pod.lastSync}. Tap to reconnect.',
          onTap: () => context.go('/device'),
        ),
      if (pod.offSkin)
        NoticeCard(
          icon: 'signal',
          title: "Atrium isn't on your skin",
          body: 'Measuring is paused to save battery. It starts again when you put it back on.',
          onTap: () => context.go('/device'),
        ),
      if (pod.onPlinth)
        NoticeCard(
          icon: 'pad',
          title: 'Change your pad before you put it back on',
          body: 'The old pad comes off to charge. Next time, try your ${s.side.other.word} temporal region.',
          onTap: () => context.pushDetail('/pad'),
        )
      else if (!kRealData && pod.alert == PodAlert.sync)
        NoticeCard(
          icon: 'storage',
          iconColor: t.red,
          title: 'Sync soon',
          body: 'The pod is holding ${pod.hoursHeld} hours of readings and keeps about $podLogCapacityHours. Keep your phone nearby.',
          onTap: () => context.go('/device'),
        )
      else if (s.padHours >= 48)
        NoticeCard(
          icon: 'pad',
          title: 'Time for a fresh pad',
          body: "It's been 2 days. Switch to your ${s.side.other.word} temporal region this time.",
          onTap: () => context.pushDetail('/pad'),
        ),
    ];

    final usual = d.usual;
    final body = empty
        ? Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              ?syncedLine,
              if (overdue) SyncDueNotice(lastSync: lastSync!, now: now, onTap: () => context.go('/device')),
              _EmptyBody(pod: pod),
            ],
          )
        : Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Padding(padding: const EdgeInsets.fromLTRB(8, 4, 8, kRealData ? 10 : 22), child: StoryText(s.isEvt ? evtStory : story, center: true)),
              ?syncedLine,
              ...notices,
              if (learning && !s.isEvt)
                Padding(
                  padding: const EdgeInsets.only(bottom: Space.m),
                  child: AtriumCard(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Expanded(child: Text('Learning your usual range', style: AtriumType.bodyStrong(t.ink))),
                            Text('Night 2 of 3', style: AtriumType.secondary(t.ink2)),
                          ],
                        ),
                        const SizedBox(height: 10),
                        Row(
                          children: [
                            for (var i = 0; i < 3; i++)
                              Expanded(
                                child: Padding(padding: const EdgeInsets.symmetric(horizontal: 2), child: Meter(i < 2 ? 1 : 0)),
                              ),
                          ],
                        ),
                        const SizedBox(height: 10),
                        Text('Scores start tomorrow morning. Keep wearing it overnight.', style: AtriumType.secondary(t.ink2)),
                      ],
                    ),
                  ),
                ),
              if (!s.isEvt)
                Row(
                  children: [
                    Expanded(
                      child: _ScoreCard(
                        icon: 'moon',
                        name: 'Sleep',
                        color: t.sleep,
                        value: last.sleepScore.toDouble(),
                        learning: learning,
                        foot: hmCompact(d.asleep),
                        footLabel: 'asleep',
                        route: '/sleep',
                      ),
                    ),
                    const SizedBox(width: Space.m),
                    Expanded(
                      child: _ScoreCard(
                        icon: 'effort',
                        name: 'Effort',
                        color: t.effort,
                        value: last.effort.toDouble(),
                        foot: '${last.active} min',
                        footLabel: 'active',
                        route: '/effort',
                      ),
                    ),
                  ],
                ),
              SectionHeader('Body', trailing: 'Trends', onTrailing: () => context.go('/trends')),
              _TileGrid(
                tiles: [
                  MetricTile(
                    icon: 'heart',
                    name: 'Resting HR',
                    color: t.heart,
                    value: '${last.rhr.round()}',
                    unit: 'bpm',
                    note: learning ? 'Last night' : 'Usual ${usual[Metric.rhr]!.lo.round()}–${usual[Metric.rhr]!.hi.round()}',
                    spark: _last14(d, Metric.rhr),
                    band: learning ? null : (usual[Metric.rhr]!.lo, usual[Metric.rhr]!.hi),
                    onTap: () => context.pushDetail('/heart'),
                  ),
                  if (!s.isEvt)
                    MetricTile(
                      icon: 'hrv',
                      name: 'HRV',
                      color: t.green,
                      value: '${last.hrv.round()}',
                      unit: 'ms',
                      note: learning ? 'Overnight average' : 'Usual ${usual[Metric.hrv]!.lo.round()}–${usual[Metric.hrv]!.hi.round()}',
                      spark: _last14(d, Metric.hrv),
                      band: learning ? null : (usual[Metric.hrv]!.lo, usual[Metric.hrv]!.hi),
                      onTap: () => context.pushDetail('/heart'),
                    ),
                  MetricTile(
                    icon: 'temp',
                    name: 'Skin temp',
                    color: t.red,
                    value: signed1(last.tempDelta),
                    unit: '°C',
                    note: learning ? 'Change since night 1' : 'Within your usual',
                    spark: _last14(d, Metric.temp),
                    band: (-0.3, 0.3),
                    onTap: () => context.pushDetail('/temperature'),
                  ),
                  if (!s.isEvt)
                    MetricTile(
                      icon: 'effort',
                      name: 'Active',
                      color: t.effort,
                      value: '${last.active}',
                      unit: 'min',
                      note: 'Daily average ${usual[Metric.active]!.mean.round()}',
                      spark: _last14(d, Metric.active),
                      onTap: () => context.pushDetail('/effort'),
                    ),
                ],
              ),
              const SectionHeader('Heart rate', trailing: 'Last 24 hours'),
              AtriumCard(
                onTap: () => context.pushDetail('/heart'),
                child: Hr24Chart(data: d, height: 140),
              ),
              FootNote('${kRealData ? lastSyncedSentence(lastSync, now) : 'Synced ${pod.lastSync}'} · For wellness, not medical use', center: true),
            ],
          );

    return Scaffold(
      backgroundColor: t.bg,
      body: PhotoScroll(
        photoHeight: (empty ? 560 : 430) + top - 60,
        child: RefreshIndicator(
          // Real builds: pull to sync the pod's log now (#20). Demo builds just settle.
          onRefresh: () async {
            if (kRealData) await ref.read(podSessionProvider.notifier).sync();
          },
          child: ListView(
            physics: const AlwaysScrollableScrollPhysics(),
            padding: EdgeInsets.only(bottom: MediaQuery.paddingOf(context).bottom + 110),
            children: [
              hero,
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: Space.gutter),
                child: body,
              ),
            ],
          ),
        ),
      ),
    );
  }

  static List<double> _last14(MetricsSource d, Metric m) => [for (final x in d.days.sublist((d.days.length - 14).clamp(0, d.days.length))) x.value(m)];
  static String _weekday(DateTime t) => const ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'][t.weekday - 1];
  static String _month(DateTime t) =>
      const ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'][t.month - 1];
}

class _ScoreCard extends StatelessWidget {
  const _ScoreCard({
    required this.icon,
    required this.name,
    required this.color,
    required this.value,
    required this.foot,
    required this.footLabel,
    required this.route,
    this.learning = false,
  });
  final String icon;
  final String name;
  final Color color;
  final double value;
  final String foot;
  final String footLabel;
  final String route;
  final bool learning;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return AtriumCard(
      onTap: () => context.pushDetail(route),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              AIcon(icon, size: 14, color: color, stroke: 2),
              const SizedBox(width: 5),
              Text(name.toUpperCase(), style: AtriumType.label(color)),
            ],
          ),
          Center(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(0, 12, 0, 8),
              child: SizedBox(
                width: 116,
                child: ArchGauge(value: value, color: color, learning: learning, small: true, strokeWidth: 14, label: name),
              ),
            ),
          ),
          Center(
            child: Text.rich(
              TextSpan(
                children: [
                  TextSpan(
                    text: foot,
                    style: AtriumType.secondary(t.ink).copyWith(fontWeight: FontWeight.w700),
                  ),
                  TextSpan(text: ' $footLabel', style: AtriumType.secondary(t.ink2)),
                ],
              ),
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
          padding: const EdgeInsets.only(bottom: Space.m),
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

class _EmptyBody extends StatelessWidget {
  const _EmptyBody({required this.pod});
  final PodStatus pod;

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      const SectionHeader('Before bed'),
      ListGroup(
        children: [
          pod.connected
              ? ListRow(
                  icon: 'bolt',
                  title: 'Pod charged',
                  subtitle: '${pod.batteryPercent}%, enough for about ${formatLife(MeasureMode.smart.lifeDays(pod.batteryPercent))}',
                  trailing: AIcon('check', size: 18, color: AtriumColors.of(context).green, stroke: 2.2),
                )
              : ListRow(
                  icon: 'bolt',
                  title: 'Charge the pod',
                  subtitle: 'Not connected. Set it on the Plinth, then open Device',
                  chevron: true,
                  onTap: () => context.go('/device'),
                ),
          ListRow(
            icon: 'pad',
            title: 'A fresh skin pad',
            subtitle: 'Change it at every charge or after 2 days',
            chevron: true,
            onTap: () => context.pushDetail('/pad'),
          ),
          const ListRow(
            icon: 'storage',
            title: 'Keep your phone in the room',
            subtitle: 'The pod holds about $podLogCapacityHours hours, so open Atrium each morning',
          ),
        ],
      ),
    ],
  );
}

/// Last 24 hours of heart rate, with last night shaded.
class Hr24Chart extends StatelessWidget {
  const Hr24Chart({required this.data, this.height = 170, super.key});
  final MetricsSource data;
  final double height;

  @override
  Widget build(BuildContext context) {
    final hr = data.hr24;
    final sStart = hr.indexWhere((p) => p.minute >= data.bedMinute - 1440);
    final sEnd = hr.indexWhere((p) => p.minute >= data.wakeMinute);
    final xl = <(int, String)>[
      for (var i = 5; i < hr.length - 4; i++)
        if ((((hr[i].minute % 1440) + 1440) % 1440) % 360 == 0) (i, '${((((hr[i].minute % 1440) + 1440) % 1440) ~/ 60).toString().padLeft(2, '0')}:00'),
    ];
    return AtriumChart(
      points: [for (final p in hr) p.bpm.toDouble()],
      color: AtriumColors.of(context).heart,
      kind: ChartKind.line,
      height: height,
      xLabels: xl,
      shade: [if (sStart >= 0 && sEnd > sStart) (sStart, sEnd)],
      labels: [for (final p in hr) clockLabel(p.minute)],
      unit: 'bpm',
      semantics: 'Heart rate over the last 24 hours',
    );
  }
}
