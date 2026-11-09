// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/routes/app_router.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/device/data/firmware_update_provider.dart';
import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';
import 'package:atrium/features/device/presentation/device_sheets.dart';
import 'package:atrium/features/device/presentation/device_widgets.dart';
import 'package:atrium/features/onboarding/presentation/bluetooth_view.dart';
import 'package:atrium/features/pod/data/pod_permissions.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:atrium/features/pod/domain/link_machine.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:atrium/features/pod/domain/sync_age.dart';
import 'package:atrium/features/pod/presentation/sync_status.dart';
import 'package:atrium/features/settings/data/log_out.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Device tab (#15-#21): the pod, its battery and pad, measurement mode and pod actions.
/// Whether background sync can run unhindered (battery optimisation, Background App Refresh); read again on return.
final backgroundAccessProvider = FutureProvider.autoDispose<bool>((ref) => PodPermissions.backgroundAllowed());

class DeviceView extends ConsumerStatefulWidget {
  const DeviceView({super.key});

  @override
  ConsumerState<DeviceView> createState() => _DeviceViewState();
}

class _DeviceViewState extends ConsumerState<DeviceView> {
  Timer? _search;
  bool _reconnecting = false;
  bool get _searching => _search != null || _reconnecting;

  @override
  void dispose() {
    _search?.cancel();
    super.dispose();
  }

  void _searchAgain() {
    if (kRealData) {
      setState(() => _reconnecting = true);
      ref.read(podSessionProvider.notifier).reconnect().then((ok) {
        if (!mounted) return;
        setState(() => _reconnecting = false);
        final name = ref.read(appStateProvider).podName ?? 'Atrium';
        showAtriumToast(context, ok ? 'Connected to $name' : "Couldn't find $name");
      });
      return;
    }
    setState(
      () => _search = Timer(const Duration(milliseconds: 1800), () {
        if (!mounted) return;
        setState(() => _search = null);
        ref.read(appStateProvider.notifier).update((s) => s.copyWith(situation: PodSituation.worn));
        showAtriumToast(context, 'Connected to Atrium 3F2A');
      }),
    );
  }

  @override
  Widget build(BuildContext context) {
    final s = ref.watch(appStateProvider);
    final t = AtriumColors.of(context);
    final d = ref.watch(podStatusProvider);
    final name = s.podName ?? (kRealData ? 'Atrium' : 'Atrium 3F2A');
    // Real builds: sync age from the saved last-sync time (#20), and the pod software from DIS and the update flow (#21).
    final lastSync = kRealData ? ref.watch(lastSyncProvider) : null;
    final now = DateTime.now();
    final overdue = kRealData && syncOverdue(lastSync, now);
    final syncedAgo = kRealData ? lastSyncedLabel(lastSync, now) : d.lastSync;
    final ota = kRealData ? ref.watch(firmwareUpdateProvider) : null;
    final podSoftware = kRealData ? (ota!.current ?? PodVersion.tryParse(ref.watch(podSessionProvider.select((p) => p.firmware)))) : null;
    final life = d.onPlinth
        ? (d.charging || d.alert == PodAlert.cold ? d.eta! : 'Ready to wear')
        : d.connected
        ? 'About ${formatLife(s.mode.lifeDays(d.batteryPercent))} left'
        : 'Last seen at ${d.lastSync}';

    final children = <Widget>[];

    // One calm notice for whatever stands between the pod and this phone, with the way to fix it.
    final phase = kRealData ? ref.watch(podSessionProvider.select((p) => p.phase)) : null;
    final background = kRealData ? ref.watch(backgroundAccessProvider).value ?? true : true;
    if (phase == LinkPhase.needsPermission) {
      children.addAll([
        NoticeCard(
          icon: 'bt',
          title: 'Bluetooth is off for Atrium',
          body: 'Atrium needs it to reach the pod and copy its readings. Tap to allow it.',
          onTap: () => showBluetoothProblem(context, BluetoothAccess.denied),
        ),
        const SizedBox(height: Space.m),
      ]);
    } else if (phase == LinkPhase.bluetoothOff) {
      children.addAll([
        const NoticeCard(icon: 'bt', title: 'Bluetooth is off', body: 'Turn it on in Control Centre or quick settings, and Atrium reconnects by itself.'),
        const SizedBox(height: Space.m),
      ]);
    } else if (!background && phase != LinkPhase.noPod) {
      children.addAll([
        NoticeCard(
          icon: 'storage',
          title: 'Background sync may wait',
          body: Platform.isIOS
              ? 'Background App Refresh is off for Atrium, so readings only copy while the app is open. Tap to turn it on.'
              : 'Battery saving can hold back syncing while Atrium is closed. Tap, then let Atrium run in the background.',
          onTap: () async {
            await PodPermissions.openBackgroundSettings();
            ref.invalidate(backgroundAccessProvider);
          },
        ),
        const SizedBox(height: Space.m),
      ]);
    }

    if (!d.connected) {
      children.addAll([
        const CutCard(image: 'pod', semanticLabel: 'The pod', dim: true),
        const SizedBox(height: Space.m),
        AtriumCard(
          padding: const EdgeInsets.fromLTRB(Space.xl, 22, Space.xl, Space.xl),
          child: Column(
            children: [
              Text(
                "Atrium isn't nearby",
                textAlign: TextAlign.center,
                style: AtriumType.serifAt(28, color: t.ink),
              ),
              const SizedBox(height: Space.s),
              Text(
                'It keeps logging on its own and holds about $podLogCapacityHours hours. Sync before then.',
                textAlign: TextAlign.center,
                style: AtriumType.body(t.ink2),
              ),
              if (kRealData) ...[const SizedBox(height: Space.s), LastSyncedLine(lastSync: lastSync, now: now, center: true)],
              const SizedBox(height: Space.l),
              _SmallButton(
                ota?.busy ?? false
                    ? 'Updating pod software…'
                    : _searching
                    ? 'Searching…'
                    : 'Search again',
                onPressed: _searching || (ota?.busy ?? false) ? null : _searchAgain,
              ),
            ],
          ),
        ),
        if (ota?.busy ?? false) ...[
          const SizedBox(height: Space.m),
          NoticeCard(
            icon: 'chip',
            title: 'Updating pod software',
            body: 'The update has the Bluetooth link for now. Atrium reconnects when the pod has restarted.',
            onTap: () => context.pushDetail('/firmware'),
          ),
        ],
        if (overdue) ...[const SizedBox(height: Space.m), SyncDueNotice(lastSync: lastSync!, now: now)],
        const SectionHeader('Things to try'),
        const ListGroup(
          children: [
            ListRow(title: 'Bring your phone within about 10 m of the pod'),
            ListRow(title: 'Check Bluetooth is on for Atrium'),
            ListRow(title: 'If the battery may be empty, take the pad off and put the pod on the Plinth'),
          ],
        ),
      ]);
    } else {
      children.add(
        CutCard(
          image: d.onPlinth ? 'pod-plinth' : 'pod',
          semanticLabel: d.onPlinth ? 'The pod on its Plinth' : 'The pod',
          caption: Column(
            children: [
              Text(
                d.line,
                textAlign: TextAlign.center,
                style: AtriumType.serifAt(24, color: t.ink),
              ),
              const SizedBox(height: 3),
              Text(
                kRealData && lastSync == null ? '$name · not synced yet' : '$name · synced ${syncedAgo.toLowerCase()}',
                textAlign: TextAlign.center,
                style: AtriumType.secondary(overdue ? t.red : t.ink2),
              ),
            ],
          ),
        ),
      );
    }

    // Alert notice (cold charging, sync due). Real builds: sync due from the last sync time, connected or not.
    if (d.connected && overdue) {
      children.addAll([const SizedBox(height: Space.m), SyncDueNotice(lastSync: lastSync!, now: now)]);
    }
    if (d.alert != null && (d.alert == PodAlert.cold || !kRealData)) {
      final cold = d.alert == PodAlert.cold;
      children.addAll([
        const SizedBox(height: Space.m),
        NoticeCard(
          icon: cold ? 'temp' : 'storage',
          iconColor: cold ? t.cool : t.red,
          title: cold ? 'Charging paused in the cold' : 'Sync soon',
          body: cold
              ? 'The pod charges between about 0 and 45 °C, and more slowly when cold. It carries on when it warms up.'
              : 'The pod is holding ${d.hoursHeld} hours of readings and keeps about $podLogCapacityHours. Keep your phone near it.',
        ),
      ]);
    } else if (d.connected) {
      children.add(const SizedBox(height: Space.m));
    }

    if (d.connected) {
      final padLine = d.onPlinth ? 'Off while charging' : 'Worn ${s.padHours} h of 48 · ${s.padsLeft} left on the card';
      children.addAll([
        IntrinsicHeight(
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Expanded(
                child: _BatteryCard(
                  name: 'Pod',
                  value: '${d.batteryPercent}',
                  unit: '%',
                  bolt: d.charging,
                  fraction: d.batteryPercent / 100,
                  barColor: d.batteryPercent <= 20 ? t.red : t.green,
                  note: life,
                ),
              ),
              const SizedBox(width: Space.m),
              Expanded(
                child: _BatteryCard(
                  name: 'Skin pad',
                  value: d.onPlinth ? 'Off' : '${math.max(0, 48 - s.padHours)}',
                  unit: d.onPlinth ? '' : 'h left',
                  fraction: d.onPlinth ? 0 : math.min(1, s.padHours / 48),
                  barColor: t.ink3,
                  note: padLine,
                  onTap: () => context.pushDetail('/pad'),
                ),
              ),
            ],
          ),
        ),
        const SectionHeader('Measurement mode'),
        AtriumCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Segmented<MeasureMode>(
                values: MeasureMode.values,
                labels: [for (final m in MeasureMode.values) m.title],
                selected: s.mode,
                onChanged: (m) =>
                    kRealData ? ref.read(podSessionProvider.notifier).setMode(m) : ref.read(appStateProvider.notifier).update((x) => x.copyWith(mode: m)),
              ),
              const SizedBox(height: Space.m),
              Text(s.mode.description, style: AtriumType.secondary(t.ink)),
              const SizedBox(height: 6),
              Text('A full charge lasts about ${formatLife(s.mode.lifeDays())} with day and night wear.', style: AtriumType.secondary(t.ink2)),
            ],
          ),
        ),
        const SectionHeader('Pod'),
        ListGroup(
          children: [
            ListRow(
              icon: 'signal',
              title: 'Skin contact',
              subtitle: d.offSkin
                  ? 'Measuring paused to save battery'
                  : d.onPlinth
                  ? 'On the Plinth'
                  : 'Measuring',
              value: d.offSkin || d.onPlinth ? 'Off skin' : 'Good',
            ),
            ListRow(icon: 'ruler', title: 'Worn on', subtitle: 'Upright, window end up', value: s.side.label),
            ListRow(icon: 'clock', title: 'Last sync', value: kRealData && lastSync == null ? 'Not yet' : syncedAgo),
            ListRow(
              icon: 'storage',
              title: 'Held on the pod',
              subtitle: "It keeps about $podLogCapacityHours hours; Atrium syncs whenever it's near",
              value: '${kRealData ? hoursSinceSync(lastSync, now) : d.hoursHeld} h',
            ),
            if (kRealData)
              ListRow(
                icon: 'chip',
                title: 'Pod software',
                subtitle: switch (ota!.phase) {
                  OtaPhase.available => '${ota.release!.version} is available',
                  OtaPhase.upToDate || OtaPhase.done => 'Up to date',
                  _ when ota.busy => 'Updating…',
                  _ => 'Check for updates',
                },
                value: podSoftware?.toString() ?? 'Unknown',
                trailing: ota.phase == OtaPhase.available ? const _UpdateBadge() : null,
                chevron: true,
                onTap: () => context.pushDetail('/firmware'),
              )
            else
              ListRow(
                icon: 'chip',
                title: 'Pod software',
                subtitle: s.firmware == FirmwareState.done ? 'Demo finished' : 'Demo · an update is available',
                trailing: s.firmware == FirmwareState.done ? null : const _UpdateBadge(),
                chevron: true,
                onTap: () => context.pushDetail('/firmware'),
              ),
            // The pod has no readable serial (privacy, ble_gatt.md section 2): real builds show only the build stage.
            const ListRow(icon: 'device', title: 'Hardware', value: kRealData ? 'EVT' : 'EVT · 3F2A'),
          ],
        ),
        const SizedBox(height: 22),
        ListGroup(
          children: [
            ListRow(title: 'Change the skin pad', onTap: () => context.pushDetail('/pad')),
            ListRow(title: 'How charging works', onTap: () => showPlinthInfo(context)),
            ListRow(title: "What's in the box", onTap: () => showBoxContents(context)),
            ListRow(title: 'Turn off for storage', onTap: () => showStorageOff(context, ref)),
            ListRow(title: 'Reset the pod', onTap: () => showResetPod(context)),
            ListRow(
              title: 'Forget this pod',
              danger: true,
              onTap: () async {
                if (!await showForgetPod(context, name)) return;
                await PhoneData.forgetPod(ref);
                if (context.mounted) context.go('/wake'); // pair a pod again: lift it from the Plinth
              },
            ),
          ],
        ),
      ]);
    }

    return PlainPage(title: 'Device', children: children);
  }
}

/// Battery or pad card: label, big number, meter, note.
class _BatteryCard extends StatelessWidget {
  const _BatteryCard({
    required this.name,
    required this.value,
    required this.unit,
    required this.fraction,
    required this.barColor,
    required this.note,
    this.bolt = false,
    this.onTap,
  });
  final String name;
  final String value;
  final String unit;
  final double fraction;
  final Color barColor;
  final String note;
  final bool bolt;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return AtriumCard(
      onTap: onTap,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(name, style: AtriumType.secondary(t.ink2)),
          const SizedBox(height: Space.s),
          Row(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              Flexible(child: BigValue(value, unit)),
              if (bolt)
                Padding(
                  padding: const EdgeInsets.only(left: 4, bottom: 4),
                  child: AIcon('bolt', size: 20, color: t.green, stroke: 2),
                ),
            ],
          ),
          const SizedBox(height: Space.s),
          Meter(fraction, color: barColor, height: 8),
          const SizedBox(height: Space.s),
          Text(note, style: AtriumType.secondary(t.ink2)),
        ],
      ),
    );
  }
}

/// "Update" marker on the Pod software row: sentence case, no caps.
class _UpdateBadge extends StatelessWidget {
  const _UpdateBadge();

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Container(
      margin: const EdgeInsets.only(left: Space.s),
      padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 4),
      decoration: BoxDecoration(color: t.track, borderRadius: BorderRadius.circular(Radii.pill)),
      child: Text(
        'Update',
        style: TextStyle(fontSize: 13, fontWeight: FontWeight.w500, height: 1.2, color: t.ink),
      ),
    );
  }
}

/// Compact pill button (the prototype's `.btn.sm`).
class _SmallButton extends StatelessWidget {
  const _SmallButton(this.label, {required this.onPressed});
  final String label;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final enabled = onPressed != null;
    return Pressable(
      onTap: onPressed ?? () {},
      child: AnimatedOpacity(
        duration: Motion.tap,
        opacity: enabled ? 1 : 0.5,
        child: Container(
          height: 36,
          padding: const EdgeInsets.symmetric(horizontal: Space.l),
          decoration: BoxDecoration(color: t.button, borderRadius: BorderRadius.circular(Radii.pill)),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [Text(label, style: AtriumType.button(t.buttonInk).copyWith(fontSize: 15))],
          ),
        ),
      ),
    );
  }
}
