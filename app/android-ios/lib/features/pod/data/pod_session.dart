// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/data/pod_permissions.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:atrium/features/pod/domain/sync_age.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

@immutable
class PodSessionState {
  const PodSessionState({
    this.link = LinkState.idle,
    this.found = const [],
    this.syncing = false,
    this.lastSync,
    this.error,
    this.firmware,
    this.handedOver = false,
  });
  final LinkState link;
  final List<FoundPod> found;
  final bool syncing;

  /// When the phone last had all the pod's readings (kept in the store across launches).
  final DateTime? lastSync;
  final String? error;

  /// DIS Firmware Revision (0x2A26) as read at the last connect, e.g. `0.1.0+41f1d65`.
  final String? firmware;

  /// True while a pod software update owns the BLE link: no reconnects, syncs or writes from the app.
  final bool handedOver;

  PodSessionState copyWith({LinkState? link, List<FoundPod>? found, bool? syncing, DateTime? lastSync, String? error, String? firmware, bool? handedOver}) =>
      PodSessionState(
        link: link ?? this.link,
        found: found ?? this.found,
        syncing: syncing ?? this.syncing,
        lastSync: lastSync ?? this.lastSync,
        error: error,
        firmware: firmware ?? this.firmware,
        handedOver: handedOver ?? this.handedOver,
      );
}

/// Real-data builds: when the phone last synced the pod's log (null before the first sync, and in demo builds).
final lastSyncProvider = Provider<DateTime?>((ref) => ref.watch(podSessionProvider.select((s) => s.lastSync)));

/// Real-data builds: the pod connection's lifecycle. On launch it reconnects to the bonded pod, reads its
/// status, keeps it live from Charge State notifications and syncs the log. Demo builds do nothing here.
final podSessionProvider = NotifierProvider<PodSession, PodSessionState>(PodSession.new);

class PodSession extends Notifier<PodSessionState> {
  final _subs = <StreamSubscription<Object?>>[];

  /// Per-connection listeners (Charge State, Live Epoch), replaced on every connect.
  final _linkSubs = <StreamSubscription<Object?>>[];

  /// Screens recompute from live epochs at most this often (a full recompute reads every stored night).
  static const refreshEvery = Duration(minutes: 5);
  DateTime? _lastRefresh;

  PodLink? get _link => ref.read(podLinkProvider);

  @override
  PodSessionState build() {
    ref.onDispose(() {
      for (final s in [..._subs, ..._linkSubs]) {
        s.cancel();
      }
    });
    final link = _link;
    if (!kRealData || link == null) return const PodSessionState();
    _subs
      ..add(
        link.states.listen((s) {
          state = state.copyWith(link: s);
          if (s == LinkState.disconnected || s == LinkState.bluetoothOff) _applyDisconnected();
        }),
      )
      ..add(
        link.found.listen((p) {
          if (state.found.every((f) => f.id != p.id)) state = state.copyWith(found: [...state.found, p]);
        }),
      );
    Future.microtask(() async {
      await _loadLastSync();
      await reconnect();
    });
    return const PodSessionState();
  }

  /// The last sync time saved with the readings, so a launch out of the pod's reach still shows its age.
  Future<void> _loadLastSync() async {
    try {
      final at = (await ref.read(epochStoreProvider.future)).lastSyncAt;
      if (at != null && (state.lastSync == null || at.isAfter(state.lastSync!))) state = state.copyWith(lastSync: at);
    } on Object {
      // No store yet (first launch): nothing synced.
    }
    _applyDisconnected();
  }

  /// Persists [at] at most every [refreshEvery] for live records, and always after a full sync.
  Future<void> _markSynced(DateTime at, {bool persist = true}) async {
    state = state.copyWith(lastSync: at);
    if (!persist) return;
    try {
      await (await ref.read(epochStoreProvider.future)).setLastSync(at);
    } on Object {
      // The time stays in memory; the next sync saves it.
    }
  }

  /// The card while the pod is out of reach: not connected, with the age of the last sync.
  void _applyDisconnected() {
    if (state.link == LinkState.connected) return;
    final last = state.lastSync;
    final now = DateTime.now();
    ref
        .read(appStateProvider.notifier)
        .update(
          (a) => a.copyWith(
            livePod: PodStatus(
              label: AppState.notConnected.label,
              batteryPercent: a.livePod?.batteryPercent ?? 0,
              onPlinth: false,
              charging: false,
              connected: false,
              line: AppState.notConnected.line,
              lastSync: last == null ? 'not yet' : lastSyncedLabel(last, now),
              hoursHeld: hoursSinceSync(last, now),
            ),
          ),
        );
  }

  /// Reconnects to the bonded pod. False when there is none or it is out of reach.
  Future<bool> reconnect() async {
    if (state.handedOver) return false;
    // Without the Bluetooth permission (not given yet, or taken back in Settings) there is nothing to try.
    if (await PodPermissions.bluetooth() != BluetoothAccess.granted) return false;
    final id = await _link?.bondedPodId();
    return id != null && await pair(id);
  }

  /// Mode 0x0601; the app keeps the choice even if the write fails, and sends it on the next connect.
  Future<void> setMode(MeasureMode m) async {
    ref.read(appStateProvider.notifier).update((s) => s.copyWith(mode: m));
    try {
      await _link?.setMode(m);
    } on Object catch (e) {
      state = state.copyWith(error: '$e');
    }
  }

  /// Ship mode (Command 0x0605 op 0x02). True when the pod took it.
  Future<bool> shipMode() async {
    try {
      await _link?.shipMode();
      return true;
    } on Object catch (e) {
      state = state.copyWith(error: podErrorText(e));
      return false;
    }
  }

  Future<void> scan() async {
    state = state.copyWith(found: const []);
    final access = await PodPermissions.requestBluetooth();
    if (access != BluetoothAccess.granted) {
      state = state.copyWith(error: access == BluetoothAccess.locationOff ? locationOffText : bluetoothPermissionText);
      return;
    }
    await _link?.startScan();
  }

  Future<void> stopScan() async => _link?.stopScan();

  /// Connects, bonds and sets the clock; then status, live updates, background sync and a first log sync.
  Future<bool> pair(String id, {String? name}) async {
    final link = _link;
    if (link == null || state.handedOver) return false;
    try {
      final (start, end) = _sleepWindow();
      await link.connect(id, sleepStartMin: start, sleepEndMin: end);
      await afterConnect(name: name);
      return true;
    } on Object catch (e) {
      state = state.copyWith(error: '$e');
      return false;
    }
  }

  /// After a connect: status, live updates (Charge State, and each new log record as it is written), background
  /// sync and one full log sync, which also fills any live records that were missed.
  Future<void> afterConnect({String? name}) async {
    final link = _link;
    if (link == null) return;
    if (name != null) ref.read(appStateProvider.notifier).update((s) => s.copyWith(podName: name));
    try {
      state = state.copyWith(firmware: await link.firmwareRevision());
    } on Object {
      // Older firmware or a read error: the version shows as unknown.
    }
    await _refreshStatus();
    await _link?.setMode(ref.read(appStateProvider).mode);
    for (final s in _linkSubs) {
      await s.cancel();
    }
    _linkSubs
      ..clear()
      ..add(link.watchChargeState().listen(_applyStatus))
      ..add(link.liveEpochs().listen(_onLiveEpoch));
    await link.setBackgroundSync();
    unawaited(sync());
  }

  /// A record the pod just logged: store it now (uptime ones wait for a sync to date them), sync if the pod's head
  /// page has moved past what any sync has read, and refresh the screens every [refreshEvery].
  Future<void> _onLiveEpoch(LiveEpoch e) async {
    if (state.handedOver) return;
    final store = await ref.read(epochStoreProvider.future);
    final added = e.epoch.isUptime ? 0 : await store.add([e.epoch]);
    if (PodLink.needsSync(headPage: e.headPage, cursor: store.syncedToPage)) {
      unawaited(sync());
      return;
    }
    if (added == 0) return;
    final now = DateTime.now();
    final due = _lastRefresh == null || now.difference(_lastRefresh!) >= refreshEvery;
    await _markSynced(now, persist: due);
    if (due) {
      _lastRefresh = now;
      ref.invalidate(metricsProvider);
    }
  }

  /// The pod's night window: last night's bed and wake times, 30 min wider each side and on a 15 min grid,
  /// or 23:00-07:00 until there is a night.
  (int, int) _sleepWindow() {
    final m = ref.read(metricsProvider);
    if (m.isEmpty || m.asleep == 0) return (PodLink.defaultSleepStart, PodLink.defaultSleepEnd);
    int snap(int v) => ((v % 1440 + 1440) % 1440) ~/ 15 * 15;
    return (snap(m.bedMinute - 30), snap(m.wakeMinute + 30));
  }

  Future<void> _refreshStatus() async {
    final s = await _link?.chargeState();
    if (s != null) _applyStatus(s);
  }

  void _applyStatus(LiveStatus s) {
    if (state.handedOver) return;
    final last = state.lastSync;
    final now = DateTime.now();
    final ago = last == null ? 'not yet' : lastSyncedLabel(last, now);
    final held = hoursSinceSync(last, now);
    ref
        .read(appStateProvider.notifier)
        .update(
          (a) => a.copyWith(
            livePod: s.toPodStatus(lastSync: ago, hoursHeld: held),
          ),
        );
  }

  Future<int> sync() async {
    if (state.syncing || state.handedOver) return 0;
    state = state.copyWith(syncing: true);
    try {
      final added = await ref.read(metricsProvider.notifier).sync();
      state = state.copyWith(syncing: false);
      await _markSynced(DateTime.now());
      await _refreshStatus();
      return added;
    } on Object catch (e) {
      state = state.copyWith(syncing: false, error: '$e');
      return 0;
    }
  }

  /// Pod software update: hand the BLE link to MCUmgr. Waits for a running sync, stops live listeners, disconnects
  /// and returns the pod's platform id (iOS peripheral UUID, Android address), which MCUmgr connects to by itself.
  Future<String> handOver() async {
    final link = _link;
    final id = await link?.bondedPodId();
    if (link == null || id == null) throw StateError('No paired pod');
    state = state.copyWith(handedOver: true);
    for (var i = 0; i < 60 && state.syncing; i++) {
      await Future<void>.delayed(const Duration(milliseconds: 500));
    }
    for (final s in _linkSubs) {
      unawaited(s.cancel());
    }
    _linkSubs.clear();
    await link.release();
    return id;
  }

  /// After the update: reconnect (retrying until [timeout]; the pod restarts and swaps images first), resume live
  /// updates and syncing, and return the firmware revision the pod reports now (null when it wasn't reached).
  Future<String?> takeBack(Duration timeout) async {
    final link = _link;
    final id = await link?.bondedPodId();
    if (link == null || id == null) {
      state = state.copyWith(handedOver: false);
      return null;
    }
    // Still handed over while retrying, so the app's own reconnects and syncs stay out of the way.
    final until = DateTime.now().add(timeout);
    while (DateTime.now().isBefore(until)) {
      final left = until.difference(DateTime.now());
      try {
        final (start, end) = _sleepWindow();
        await link.connect(id, sleepStartMin: start, sleepEndMin: end).timeout(left);
        final rev = await link.firmwareRevision();
        state = state.copyWith(handedOver: false);
        await afterConnect();
        return rev;
      } on Object {
        // Not advertising yet (restart and image swap), or out of reach. Cancel a pending connect, then try again.
        try {
          await link.disconnect();
        } on Object {
          // Nothing to cancel.
        }
        if (DateTime.now().isBefore(until)) await Future<void>.delayed(const Duration(seconds: 3));
      }
    }
    state = state.copyWith(handedOver: false);
    return null;
  }
}
