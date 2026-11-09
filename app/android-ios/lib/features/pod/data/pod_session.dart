// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/data/pod_permissions.dart';
import 'package:atrium/features/pod/data/spool.dart';
import 'package:atrium/features/pod/domain/link_machine.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:atrium/features/pod/domain/sync_age.dart';
import 'package:atrium/features/pod/domain/sync_runner.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart' show AppLifecycleListener;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';

/// The one source of truth for the pod: the link phase ([LinkPhase]), the sync ([SyncStatus]), the last Charge State
/// and the firmware revision. Screens read [podStatusProvider], [lastSyncProvider] and this; nothing else stores pod
/// state. The pod software update's own phases live in `firmwareUpdateProvider`, which borrows the link through
/// [PodSession.handOver] and [PodSession.takeBack].
@immutable
class PodSessionState {
  const PodSessionState({
    this.phase = LinkPhase.noPod,
    this.found = const [],
    this.sync = const SyncStatus(),
    this.status,
    this.error,
    this.firmware,
    this.liveBpm,
  });
  final LinkPhase phase;

  /// Heart Rate Measurement (0x2A37), 1 Hz while connected, on skin and the app is in front; null otherwise.
  final int? liveBpm;
  final List<FoundPod> found;
  final SyncStatus sync;

  /// The last Charge State (0x0101) read or notified on this connection; null while not connected.
  final LiveStatus? status;
  final String? error;

  /// DIS Firmware Revision (0x2A26) as read at the last connect, e.g. `0.1.0+41f1d65`.
  final String? firmware;

  bool get connected => phase == LinkPhase.connected;

  /// True while a pod software update owns the BLE link.
  bool get handedOver => phase == LinkPhase.handedOver;
  bool get syncing => sync.syncing;

  /// When the phone last had all the pod's readings (kept in the store across launches).
  DateTime? get lastSync => sync.lastSync;

  PodSessionState copyWith({
    LinkPhase? phase,
    List<FoundPod>? found,
    SyncStatus? sync,
    LiveStatus? status,
    bool clearStatus = false,
    String? error,
    String? firmware,
    int? liveBpm,
    bool clearBpm = false,
  }) => PodSessionState(
    phase: phase ?? this.phase,
    found: found ?? this.found,
    sync: sync ?? this.sync,
    status: clearStatus ? null : status ?? this.status,
    error: error,
    firmware: firmware ?? this.firmware,
    liveBpm: clearStatus || clearBpm ? null : liveBpm ?? this.liveBpm,
  );
}

/// Real-data builds: when the phone last synced the pod's log (null before the first sync, and in demo builds).
final lastSyncProvider = Provider<DateTime?>((ref) => ref.watch(podSessionProvider.select((s) => s.lastSync)));

/// The pod card every screen shows: the demo situation, or derived from the session (never stored separately).
final podStatusProvider = Provider<PodStatus>((ref) {
  if (!kRealData) return PodStatus.demo[ref.watch(appStateProvider.select((a) => a.situation))]!;
  final s = ref.watch(podSessionProvider);
  return podStatusOf(s, DateTime.now());
});

/// The card for a session: live while connected, otherwise "not nearby" with the age of the last sync.
PodStatus podStatusOf(PodSessionState s, DateTime now) {
  final last = s.lastSync;
  final ago = last == null ? 'not yet' : lastSyncedLabel(last, now);
  final held = hoursSinceSync(last, now);
  final live = s.status;
  if (s.connected && live != null) return live.toPodStatus(lastSync: ago, hoursHeld: held);
  return PodStatus(
    label: s.phase == LinkPhase.handedOver ? 'Updating' : AppState.notConnected.label,
    batteryPercent: live == null ? 0 : ((live.socPercent / 10).round() * 10).clamp(0, 100),
    onPlinth: false,
    charging: false,
    connected: false,
    line: switch (s.phase) {
      LinkPhase.bluetoothOff => 'Bluetooth is off',
      LinkPhase.needsPermission => 'Bluetooth not allowed',
      LinkPhase.connecting => 'Connecting',
      LinkPhase.handedOver => 'Installing pod software',
      _ => AppState.notConnected.line,
    },
    lastSync: ago,
    hoursHeld: held,
  );
}

/// Real-data builds: the pod connection's lifecycle. On launch it takes in what background sync left, reconnects to
/// the bonded pod, keeps its status live from Charge State notifications and syncs the log. Demo builds do nothing.
final podSessionProvider = NotifierProvider<PodSession, PodSessionState>(PodSession.new);

class PodSession extends Notifier<PodSessionState> {
  final _subs = <StreamSubscription<Object?>>[];

  /// Per-connection listeners (Charge State, Live Epoch), replaced on every connect.
  final _linkSubs = <StreamSubscription<Object?>>[];

  /// Live epochs are written to the store in batches of this many (one every 30 s, so about 2 min); a sync, the app
  /// going to the background or a dispose writes what is waiting. A lost batch costs nothing: the cursor only moves
  /// with a full sync, which reads those pages again.
  static const liveBatch = 4;

  DateTime? _lastRefresh;
  final _pendingLive = <TimedEpoch>[];
  Timer? _retry;
  int _attempt = 0;
  bool _disposed = false;

  /// True while the app is in front; background connects are left to native background sync.
  bool _foreground = true;
  late SyncRunner _runner;
  AppLifecycleListener? _life;
  StreamSubscription<int>? _hr;

  PodLink? get _link => ref.read(podLinkProvider);

  @override
  PodSessionState build() {
    _runner = SyncRunner(_syncOnce, onChange: (s) => _set(state.copyWith(sync: s, error: state.error)));
    ref.onDispose(_dispose);
    final link = _link;
    if (!kRealData || link == null) return const PodSessionState();
    _subs
      ..add(link.states.listen(_onNative))
      ..add(
        link.found.listen((p) {
          if (state.found.every((f) => f.id != p.id)) _set(state.copyWith(found: [...state.found, p]));
        }),
      )
      ..add(link.syncProgress.listen((p) => _runner.progress(p.pagesDone, p.pagesTotal)));
    _life = AppLifecycleListener(onResume: _onResume, onPause: _onPause);
    Future.microtask(() async {
      await _restore();
      await reconnect();
    });
    return const PodSessionState();
  }

  void _dispose() {
    _disposed = true;
    _retry?.cancel();
    _runner.cancel();
    _life?.dispose();
    unawaited(_hr?.cancel());
    unawaited(_flushLive());
    for (final s in [..._subs, ..._linkSubs]) {
      unawaited(s.cancel());
    }
  }

  /// Every state change goes through here, so nothing lands after the provider is gone.
  void _set(PodSessionState s) {
    if (!_disposed) state = s;
  }

  void _event(LinkEvent e) {
    final next = nextLink(state.phase, e);
    if (next != state.phase) _set(state.copyWith(phase: next, clearStatus: next != LinkPhase.connected, error: state.error));
  }

  /// Native link events. A drop schedules a reconnect; a connect the app didn't start (iOS completing a pending
  /// connect, or state restoration) is set up like any other while the app is in front.
  void _onNative(LinkState s) {
    switch (s) {
      case LinkState.connected:
        final wasConnecting = state.phase == LinkPhase.connecting;
        _event(LinkEvent.connected);
        if (!wasConnecting && !state.handedOver && _foreground) unawaited(_setUp());
      case LinkState.disconnected:
        _event(LinkEvent.dropped);
        _scheduleReconnect();
      case LinkState.bluetoothOff:
        _event(LinkEvent.bluetoothOff);
      case LinkState.unauthorized:
        _event(LinkEvent.permissionDenied);
      case LinkState.idle:
        if (state.phase == LinkPhase.bluetoothOff) {
          _event(LinkEvent.bluetoothOn);
          _scheduleReconnect(now: true);
        }
      case LinkState.connecting || LinkState.scanning:
        break;
    }
  }

  /// The store's last sync time and anything native background sync left, so a launch out of the pod's reach still
  /// shows current readings and the age of the last sync.
  Future<void> _restore() async {
    try {
      final store = await ref.read(epochStoreProvider.future);
      _runner.restore(store.lastSyncAt);
      await ingestBackground();
    } on Object {
      // No store yet (first launch): nothing synced.
    }
  }

  /// Takes in pages native background sync downloaded while the app wasn't running.
  Future<int> ingestBackground() async {
    try {
      final store = await ref.read(epochStoreProvider.future);
      final dir = Spool.dirIn((await getApplicationSupportDirectory()).path);
      final (added, at) = await Spool.ingest(dir, store);
      if (at != null) _runner.restore(at);
      if (added > 0) ref.invalidate(metricsProvider);
      return added;
    } on Object {
      return 0;
    }
  }

  void _onResume() {
    _foreground = true;
    unawaited(() async {
      await ingestBackground();
      if (state.connected) {
        _watchHeartRate();
        unawaited(sync());
      } else if (state.phase == LinkPhase.waiting || state.phase == LinkPhase.needsPermission) {
        _attempt = 0;
        await reconnect();
      }
    }());
  }

  void _onPause() {
    _foreground = false;
    unawaited(_flushLive());
    _stopHeartRate();
  }

  /// Live pulse for Today and Heart, only while someone can see it: 1 Hz notifications cost both batteries.
  void _watchHeartRate() {
    final link = _link;
    if (link == null || _hr != null || !_foreground) return;
    _hr = link.heartRate().listen((bpm) => _set(state.copyWith(liveBpm: bpm > 0 ? bpm : null, clearBpm: bpm <= 0, error: state.error)));
  }

  void _stopHeartRate() {
    final hr = _hr;
    if (hr == null) return;
    _hr = null;
    unawaited(hr.cancel());
    if (state.connected) unawaited(_link?.stopHeartRate().catchError((Object _) {}));
    _set(state.copyWith(clearBpm: true, error: state.error));
  }

  void _scheduleReconnect({bool now = false}) {
    _retry?.cancel();
    if (_disposed || !shouldReconnect(state.phase) || !_foreground) return;
    final wait = now ? Duration.zero : reconnectDelay(_attempt++);
    _retry = Timer(wait, () => unawaited(reconnect()));
  }

  /// Reconnects to the bonded pod. False when there is none, it is out of reach, or the link is busy.
  Future<bool> reconnect() async {
    if (_disposed || state.handedOver || state.connected || state.phase == LinkPhase.connecting) return state.connected;
    // Without the Bluetooth permission (not given yet, or taken back in Settings) there is nothing to try.
    if (await PodPermissions.bluetooth() != BluetoothAccess.granted) {
      _event(LinkEvent.permissionDenied);
      return false;
    }
    final id = await _link?.bondedPodId();
    if (id == null) {
      _event(LinkEvent.forgotten);
      return false;
    }
    if (state.phase == LinkPhase.noPod || state.phase == LinkPhase.needsPermission) _set(state.copyWith(phase: LinkPhase.waiting));
    final ok = await pair(id);
    if (!ok) _scheduleReconnect();
    return ok;
  }

  /// Mode 0x0601; the app keeps the choice even if the write fails, and sends it on the next connect.
  Future<void> setMode(MeasureMode m) async {
    ref.read(appStateProvider.notifier).update((s) => s.copyWith(mode: m));
    if (!linkUsable(state.phase)) return;
    try {
      await _link?.setMode(m);
    } on Object catch (e) {
      _set(state.copyWith(error: podErrorText(e)));
    }
  }

  /// Ship mode (Command 0x0605 op 0x02). True when the pod took it.
  Future<bool> shipMode() async {
    try {
      await _link?.shipMode();
      return true;
    } on Object catch (e) {
      _set(state.copyWith(error: podErrorText(e)));
      return false;
    }
  }

  Future<void> scan() async {
    _set(state.copyWith(found: const []));
    final access = await PodPermissions.requestBluetooth();
    if (access != BluetoothAccess.granted) {
      _event(LinkEvent.permissionDenied);
      _set(state.copyWith(error: access == BluetoothAccess.locationOff ? locationOffText : bluetoothPermissionText));
      return;
    }
    _event(LinkEvent.permissionGranted);
    await _link?.startScan();
  }

  Future<void> stopScan() async => _link?.stopScan();

  /// Connects (bonding the first time) and sets the clock; then status, live updates, background sync and a log sync.
  Future<bool> pair(String id, {String? name}) async {
    final link = _link;
    if (link == null || state.handedOver) return false;
    _event(LinkEvent.connecting);
    try {
      final (start, end) = _sleepWindow();
      await link.connect(id, sleepStartMin: start, sleepEndMin: end);
      _event(LinkEvent.connected);
      await afterConnect(name: name);
      return true;
    } on Object catch (e) {
      _event(LinkEvent.dropped);
      _set(state.copyWith(error: podErrorText(e)));
      return false;
    }
  }

  /// A connect native completed by itself: set the clock and sleep window, then the usual setup.
  Future<void> _setUp() async {
    final link = _link;
    if (link == null) return;
    try {
      await link.setClock();
      final (start, end) = _sleepWindow();
      await link.setSleepWindow(start, end);
      await afterConnect();
    } on Object catch (e) {
      _set(state.copyWith(error: podErrorText(e)));
    }
  }

  /// After a connect: status, live updates (Charge State, and each new log record as it is written), background
  /// sync and one full log sync, which also fills any live records that were missed.
  Future<void> afterConnect({String? name}) async {
    final link = _link;
    if (link == null) return;
    _attempt = 0;
    if (state.phase != LinkPhase.connected) _set(state.copyWith(phase: LinkPhase.connected));
    if (name != null) ref.read(appStateProvider.notifier).update((s) => s.copyWith(podName: name));
    try {
      _set(state.copyWith(firmware: await link.firmwareRevision()));
    } on Object {
      // Older firmware or a read error: the version shows as unknown.
    }
    await _refreshStatus();
    try {
      await link.setMode(ref.read(appStateProvider).mode);
    } on Object {
      // Sent again on the next connect.
    }
    for (final s in _linkSubs) {
      await s.cancel();
    }
    _linkSubs
      ..clear()
      ..add(link.watchChargeState().listen(_applyStatus))
      ..add(link.liveEpochs().listen(_onLiveEpoch));
    unawaited(_hr?.cancel());
    _hr = null;
    _watchHeartRate();
    unawaited(_enableBackground(link));
    unawaited(sync());
  }

  /// Not awaited: nothing on the connect path waits for the data folder.
  Future<void> _enableBackground(PodLink link) async {
    try {
      await link.setBackgroundSync(storeDir: (await getApplicationSupportDirectory()).path);
    } on Object {
      // Platforms without the plugin: foreground sync only.
    }
  }

  /// A record the pod just logged: queue it (uptime ones wait for a sync to date them), sync if the pod's head page
  /// has moved past what any sync has read, and refresh the screens now and then.
  Future<void> _onLiveEpoch(LiveEpoch e) async {
    if (state.handedOver) return;
    final store = await ref.read(epochStoreProvider.future);
    if (PodLink.needsSync(headPage: e.headPage, cursor: store.syncedToPage)) {
      unawaited(sync());
      return;
    }
    if (e.epoch.isUptime) return;
    _pendingLive.add(e.epoch);
    if (_pendingLive.length < liveBatch) return;
    final added = await _flushLive();
    if (added == 0) return;
    final now = DateTime.now();
    _runner.touched(now);
    // Screens recompute from live readings at most as often as the phone's model plan says (a sync always does).
    if (_lastRefresh == null || now.difference(_lastRefresh!) >= ref.read(modelPlanProvider).liveRefresh) {
      _lastRefresh = now;
      unawaited(_persistLastSync(now));
      ref.invalidate(metricsProvider);
    }
  }

  Future<int> _flushLive() async {
    if (_pendingLive.isEmpty) return 0;
    final batch = [..._pendingLive];
    _pendingLive.clear();
    try {
      return await (await ref.read(epochStoreProvider.future)).add(batch);
    } on Object {
      return 0;
    }
  }

  Future<void> _persistLastSync(DateTime at) async {
    try {
      await (await ref.read(epochStoreProvider.future)).setLastSync(at);
    } on Object {
      // The time stays in memory; the next sync saves it.
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
    try {
      final s = await _link?.chargeState();
      if (s != null) _applyStatus(s);
    } on Object {
      // Kept from the last notification.
    }
  }

  void _applyStatus(LiveStatus s) {
    if (!state.handedOver) _set(state.copyWith(status: s, error: state.error));
  }

  /// One sync now (or one more after the one running). Returns the readings added.
  Future<int> sync() {
    if (!linkUsable(state.phase)) return Future.value(0);
    return _runner.request();
  }

  Future<int> _syncOnce() async {
    if (!linkUsable(state.phase)) return 0;
    await _flushLive();
    final added = await ref.read(metricsProvider.notifier).sync();
    unawaited(_persistLastSync(DateTime.now()));
    await _refreshStatus();
    return added;
  }

  /// Pod software update: hand the BLE link to MCUmgr. Waits for a running sync, stops live listeners, disconnects
  /// and returns the pod's platform id (iOS peripheral UUID, Android address), which MCUmgr connects to by itself.
  Future<String> handOver() async {
    final link = _link;
    final id = await link?.bondedPodId();
    if (link == null || id == null) throw StateError('No paired pod');
    _retry?.cancel();
    await _runner.idle();
    _event(LinkEvent.handOver);
    await _flushLive();
    for (final s in _linkSubs) {
      unawaited(s.cancel());
    }
    _linkSubs.clear();
    _stopHeartRate();
    await link.release();
    return id;
  }

  /// After the update: reconnect (retrying until [timeout]; the pod restarts and swaps images first), resume live
  /// updates and syncing, and return the firmware revision the pod reports now (null when it wasn't reached).
  Future<String?> takeBack(Duration timeout) async {
    final link = _link;
    final id = await link?.bondedPodId();
    if (link == null || id == null) {
      _event(LinkEvent.takeBack);
      _event(LinkEvent.dropped);
      return null;
    }
    // Still handed over while retrying, so the app's own reconnects and syncs stay out of the way.
    final until = DateTime.now().add(timeout);
    while (DateTime.now().isBefore(until) && !_disposed) {
      final left = until.difference(DateTime.now());
      try {
        final (start, end) = _sleepWindow();
        await link.connect(id, sleepStartMin: start, sleepEndMin: end).timeout(left);
        final rev = await link.firmwareRevision();
        _event(LinkEvent.takeBack);
        _event(LinkEvent.connected);
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
    _event(LinkEvent.takeBack);
    _event(LinkEvent.dropped);
    _scheduleReconnect();
    return null;
  }
}
