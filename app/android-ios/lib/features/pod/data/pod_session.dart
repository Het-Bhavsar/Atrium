import 'dart:async';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/data/pod_permissions.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

@immutable
class PodSessionState {
  const PodSessionState({this.link = LinkState.idle, this.found = const [], this.syncing = false, this.lastSync, this.error});
  final LinkState link;
  final List<FoundPod> found;
  final bool syncing;
  final DateTime? lastSync;
  final String? error;

  PodSessionState copyWith({LinkState? link, List<FoundPod>? found, bool? syncing, DateTime? lastSync, String? error}) =>
      PodSessionState(link: link ?? this.link, found: found ?? this.found, syncing: syncing ?? this.syncing, lastSync: lastSync ?? this.lastSync, error: error);
}

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
      ..add(link.states.listen((s) => state = state.copyWith(link: s)))
      ..add(
        link.found.listen((p) {
          if (state.found.every((f) => f.id != p.id)) state = state.copyWith(found: [...state.found, p]);
        }),
      );
    Future.microtask(reconnect);
    return const PodSessionState();
  }

  /// Reconnects to the bonded pod. False when there is none or it is out of reach.
  Future<bool> reconnect() async {
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
    if (link == null) return false;
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
    final store = await ref.read(epochStoreProvider.future);
    final added = e.epoch.isUptime ? 0 : await store.add([e.epoch]);
    if (PodLink.needsSync(headPage: e.headPage, cursor: store.syncedToPage)) {
      unawaited(sync());
      return;
    }
    if (added == 0) return;
    state = state.copyWith(lastSync: DateTime.now());
    final now = DateTime.now();
    if (_lastRefresh == null || now.difference(_lastRefresh!) >= refreshEvery) {
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
    final last = state.lastSync;
    final ago = last == null ? 'not yet' : _ago(DateTime.now().difference(last));
    final held = last == null ? 0 : DateTime.now().difference(last).inHours;
    ref
        .read(appStateProvider.notifier)
        .update(
          (a) => a.copyWith(
            livePod: s.toPodStatus(lastSync: ago, hoursHeld: held),
          ),
        );
  }

  Future<int> sync() async {
    if (state.syncing) return 0;
    state = state.copyWith(syncing: true);
    try {
      final added = await ref.read(metricsProvider.notifier).sync();
      state = state.copyWith(syncing: false, lastSync: DateTime.now());
      await _refreshStatus();
      return added;
    } on Object catch (e) {
      state = state.copyWith(syncing: false, error: '$e');
      return 0;
    }
  }

  static String _ago(Duration d) => d.inMinutes < 1
      ? 'Just now'
      : d.inMinutes < 60
      ? '${d.inMinutes} min ago'
      : '${d.inHours} h ago';
}
