// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:math' as math;

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/features/device/data/firmware_update_provider.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:atrium/features/research/data/raw_store.dart';
import 'package:atrium/features/research/domain/raw_packets.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart' show AppLifecycleListener;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';

/// The settings key for "Raw signal recording" (off by default).
const String rawRecordingKey = 'rawRecording';

/// What the setting does, in Settings. The battery figure (README, "Raw recordings"): the stream is about 165 B/s in
/// smart (PPG 75 + headers 6, motion 83), costed at 0.11 uA per B/s from the gap between research modes 8 and 9 in
/// `firmware/src/power/modes.c` (35 uA for 325 B/s at the same interval): about 18 uA, 8 % on smart's 215 uA and 14 %
/// on sleep's 129 uA; over a day of 16 h smart and 8 h sleep, about 9 % more current, 8 h of a 4-day charge.
const String rawRecordingNote =
    "Streams the pod's raw light and motion signals to this phone while it's connected, and saves them here for research. "
    'The pod keeps no raw signals itself, so nothing is recorded while it is out of reach or the phone has closed the app. '
    "Streaming costs the pod's battery: an estimated 8 % more in the day and up to 15 % more at night, about 8 hours less "
    'of a 4-day charge if it records all the time (from the firmware power budget, not yet measured). Not a medical measurement.';

/// Where raw packets come from: the pod link, or the demo build's synthetic signal.
abstract interface class RawSource {
  bool get demo;
  bool get isConnected;

  /// True on each connect (first and every reconnect), false on each drop.
  Stream<bool> get connected;

  /// (characteristic, notification bytes) as received.
  Stream<(int, Uint8List)> get packets;

  /// Turns the pod's raw PPG and accelerometer streams on (PPG Control bits 0 and 1).
  Future<void> start();
  Future<void> stop();
}

/// The real pod. Streams stop on the pod's side at every disconnect; [RawRecorder] starts them again on each connect.
class PodRawSource implements RawSource {
  PodRawSource(this.link);
  final PodLink link;

  @override
  bool get demo => false;
  @override
  bool get isConnected => link.linkState == LinkState.connected;
  @override
  Stream<bool> get connected => link.states.map((s) => s == LinkState.connected).distinct();
  @override
  Stream<(int, Uint8List)> get packets => link.rawPackets();
  @override
  Future<void> start() => link.startRawStreams();
  @override
  Future<void> stop() => link.stopRawStreams();
}

/// The demo build has no pod: while the app is open, it plays a synthetic signal in the pod's exact wire format (smart
/// mode: green at 25 Hz in notifications of 32 items, the accelerometer at 12.5 Hz in notifications of 12), so the
/// recording, the session list and the export work end to end. Sessions are marked as demo.
class DemoRawSource implements RawSource {
  DemoRawSource() {
    _life = AppLifecycleListener(onResume: () => _setConnected(true), onPause: () => _setConnected(false));
  }

  late final AppLifecycleListener _life;
  final _connected = StreamController<bool>.broadcast();
  final _packets = StreamController<(int, Uint8List)>.broadcast();
  final _clock = Stopwatch()..start();
  final _rng = math.Random(7);
  bool _isConnected = true;
  Timer? _ppg;
  Timer? _motion;
  int _ppgSeq = 0;
  int _motionSeq = 0;

  @override
  bool get demo => true;
  @override
  bool get isConnected => _isConnected;
  @override
  Stream<bool> get connected => _connected.stream;
  @override
  Stream<(int, Uint8List)> get packets => _packets.stream;

  void _setConnected(bool on) {
    if (on == _isConnected) return;
    _isConnected = on;
    if (!on) unawaited(stop());
    _connected.add(on);
  }

  @override
  Future<void> start() async {
    _ppg ??= Timer.periodic(
      const Duration(milliseconds: 1280),
      (_) => _packets.add((RawChar.ppgData, demoPpgPacket(_ppgSeq++, _clock.elapsedMilliseconds, _rng))),
    );
    _motion ??= Timer.periodic(
      const Duration(milliseconds: 960),
      (_) => _packets.add((RawChar.motionData, demoMotionPacket(_motionSeq++, _clock.elapsedMilliseconds, _rng))),
    );
  }

  @override
  Future<void> stop() async {
    _ppg?.cancel();
    _motion?.cancel();
    _ppg = _motion = null;
  }

  void dispose() {
    unawaited(stop());
    _life.dispose();
  }
}

/// A PPG Data notification of 32 green items at 25 Hz: a 62 bpm pulse dip on a slowly breathing baseline.
@visibleForTesting
Uint8List demoPpgPacket(int seq, int uptimeMs, math.Random rng) {
  final b = ByteData(8 + 32 * 3)
    ..setUint16(0, seq & 0xFFFF, Endian.little)
    ..setUint8(2, 25)
    ..setUint8(3, 1)
    ..setUint32(4, uptimeMs, Endian.little);
  for (var i = 0; i < 32; i++) {
    final t = (seq * 32 + i) / 25.0;
    final beat = (t * 62 / 60) % 1.0;
    final pulse = math.exp(-math.pow((beat - 0.25) / 0.08, 2)) + 0.35 * math.exp(-math.pow((beat - 0.55) / 0.1, 2));
    final v = (180000 + 1500 * math.sin(2 * math.pi * t / 4.5) - 2600 * pulse + 40 * (rng.nextDouble() - 0.5)).round().clamp(0, 0x7FFFF);
    final w = 0x01 << 19 | v;
    b
      ..setUint8(8 + i * 3, w >> 16)
      ..setUint8(9 + i * 3, w >> 8 & 0xFF)
      ..setUint8(10 + i * 3, w & 0xFF);
  }
  return b.buffer.asUint8List();
}

/// A Motion Data notification: 12 samples at 12.5 Hz of a still head, gravity mostly on x.
@visibleForTesting
Uint8List demoMotionPacket(int seq, int uptimeMs, math.Random rng) {
  final b = ByteData(80)
    ..setUint16(0, seq & 0xFFFF, Endian.little)
    ..setUint16(2, 12500, Endian.little)
    ..setUint32(4, uptimeMs, Endian.little);
  for (var i = 0; i < 12; i++) {
    int n(int base) => base + (rng.nextDouble() * 16 - 8).round();
    b
      ..setInt16(8 + i * 6, n(980), Endian.little)
      ..setInt16(10 + i * 6, n(-60), Endian.little)
      ..setInt16(12 + i * 6, n(150), Endian.little);
  }
  return b.buffer.asUint8List();
}

@immutable
class RawRecorderState {
  const RawRecorderState({this.enabled = false, this.current, this.capReached = false, this.listVersion = 0, this.error});
  final bool enabled;

  /// The session being written, refreshed every few seconds; null when not recording.
  final RawSession? current;

  /// Recording stopped at the size limit; it starts again once recordings are exported and deleted.
  final bool capReached;

  /// Bumped whenever the session list changes (a session ended, one was deleted).
  final int listVersion;
  final String? error;

  bool get recording => current != null;

  RawRecorderState copyWith({bool? enabled, RawSession? current, bool clearCurrent = false, bool? capReached, int? listVersion, String? error}) =>
      RawRecorderState(
        enabled: enabled ?? this.enabled,
        current: clearCurrent ? null : (current ?? this.current),
        capReached: capReached ?? this.capReached,
        listVersion: listVersion ?? this.listVersion,
        error: error,
      );
}

final rawStoreProvider = FutureProvider<RawStore>((ref) async => RawStore(RawStore.dirIn((await getApplicationSupportDirectory()).path)));

/// Null where there is nothing to record from (a real build before the link exists).
final rawSourceProvider = Provider<RawSource?>((ref) {
  if (!kRealData) {
    final s = DemoRawSource();
    ref.onDispose(s.dispose);
    return s;
  }
  final link = ref.watch(podLinkProvider);
  return link == null ? null : PodRawSource(link);
});

/// The sessions on this phone, newest first; refreshed when [RawRecorderState.listVersion] moves.
final rawSessionsProvider = FutureProvider<List<RawSession>>((ref) async {
  final open = ref.watch(rawRecorderProvider.select((s) => s.current?.id));
  ref.watch(rawRecorderProvider.select((s) => s.listVersion));
  final store = await ref.watch(rawStoreProvider.future);
  return store.list(openId: open);
});

final rawRecorderProvider = NotifierProvider<RawRecorder, RawRecorderState>(RawRecorder.new);

/// Raw signal recording: while the setting is on and the pod is connected, one session file per connection.
///
/// On every connect it asks for both raw streams (after [startDelay], so the connect's own clock and sleep-window
/// writes go first) and appends each notification as received. A drop ends the session. Before a write would pass the
/// store's limit it stops, turns the streams off and says so; nothing is deleted without the user.
class RawRecorder extends Notifier<RawRecorderState> {
  RawSessionWriter? _writer;
  StreamSubscription<(int, Uint8List)>? _packets;
  StreamSubscription<bool>? _links;
  Timer? _tick;
  Timer? _delayed;
  bool _starting = false;

  static const startDelay = Duration(milliseconds: 1500);
  static const refresh = Duration(seconds: 5);

  RawSource? get _source => ref.read(rawSourceProvider);

  @override
  RawRecorderState build() {
    final on = ref.read(appStateProvider).toggles[rawRecordingKey] ?? false;
    ref
      ..listen(appStateProvider.select((s) => s.toggles[rawRecordingKey] ?? false), (_, next) => unawaited(_apply(next)))
      ..onDispose(_dispose);
    _links = _source?.connected.listen(_onConnected);
    // A pod software update owns the link; once it is done (and the app has the link back), record again.
    if (kRealData) {
      ref.listen(firmwareUpdateProvider.select((s) => s.busy), (was, busy) {
        if ((was ?? false) && !busy) unawaited(_start());
      });
    }
    if (on) Future.microtask(() => _apply(true));
    return RawRecorderState(enabled: on);
  }

  void _dispose() {
    _delayed?.cancel();
    unawaited(_links?.cancel());
    _close();
  }

  /// A pod software update owns the link; nothing is recorded then.
  bool get _blocked {
    if (!kRealData) return false;
    try {
      return ref.read(podSessionProvider).handedOver || ref.read(firmwareUpdateProvider).busy;
    } on Object {
      return false;
    }
  }

  Future<void> _apply(bool on) async {
    state = state.copyWith(enabled: on, error: state.error);
    if (on) {
      if (_source?.isConnected ?? false) await _start();
    } else {
      _delayed?.cancel();
      await _stop(stopStreams: true);
    }
  }

  void _onConnected(bool up) {
    if (up) {
      if (!state.enabled) return;
      _delayed?.cancel();
      _delayed = Timer(startDelay, () => unawaited(_start()));
    } else {
      _delayed?.cancel();
      // The pod stops its streams by itself on a disconnect.
      _close();
    }
  }

  Future<void> _start() async {
    final source = _source;
    if (_writer != null || _starting || source == null || !state.enabled || !source.isConnected || _blocked) return;
    _starting = true;
    try {
      final store = await ref.read(rawStoreProvider.future);
      if (store.usedBytes() >= store.capBytes) {
        state = state.copyWith(capReached: true, error: state.error);
        return;
      }
      final w = store.open(DateTime.now().millisecondsSinceEpoch, demo: source.demo);
      _writer = w;
      _packets = source.packets.listen(_onPacket);
      await source.start();
      _tick = Timer.periodic(refresh, (_) {
        final cur = _writer;
        if (cur == null) return;
        cur.flush();
        state = state.copyWith(current: cur.session, error: state.error);
      });
      state = state.copyWith(current: w.session, capReached: false, listVersion: state.listVersion + 1);
    } on Object catch (e) {
      _close();
      state = state.copyWith(error: "Raw recording couldn't start: $e");
    } finally {
      _starting = false;
    }
  }

  void _onPacket((int, Uint8List) p) {
    final w = _writer;
    if (w == null) return;
    try {
      w.add(p.$1, p.$2, DateTime.now().millisecondsSinceEpoch);
    } on RawCapReached {
      state = state.copyWith(capReached: true, error: state.error);
      unawaited(_stop(stopStreams: true));
    }
  }

  Future<void> _stop({required bool stopStreams}) async {
    final had = _writer != null;
    _close();
    if (stopStreams && had && (_source?.isConnected ?? false)) {
      try {
        await _source?.stop();
      } on Object {
        // The link went down meanwhile: the pod stopped the streams itself.
      }
    }
  }

  void _close() {
    _tick?.cancel();
    _tick = null;
    unawaited(_packets?.cancel());
    _packets = null;
    final w = _writer;
    _writer = null;
    if (w == null) return;
    try {
      w.close();
    } on Object {
      // A full disk: what was flushed stays.
    }
    if (ref.mounted) state = state.copyWith(clearCurrent: true, listVersion: state.listVersion + 1, error: state.error);
  }

  /// After recordings were deleted: clear the limit notice and, if the pod is connected, record again.
  Future<void> storageChanged({RawStore? store}) async {
    final RawStore s = store ?? await ref.read<Future<RawStore>>(rawStoreProvider.future);
    final under = s.usedBytes() < s.capBytes;
    state = state.copyWith(capReached: !under && state.capReached, listVersion: state.listVersion + 1, error: state.error);
    if (under && state.enabled && (_source?.isConnected ?? false)) await _start();
  }

  /// Deletes one session (not the one being written).
  Future<void> delete(RawSession s) async {
    if (s.id == _writer?.id) return;
    await (await ref.read(rawStoreProvider.future)).delete(s);
    await storageChanged();
  }

  /// Deletes every finished session; with [includeCurrent], ends the current one first and deletes it too. [store]
  /// names the folder when the caller already knows it (log out, which runs before any recording may have started).
  Future<void> deleteAll({bool includeCurrent = false, RawStore? store}) async {
    if (includeCurrent) await _stop(stopStreams: false);
    final RawStore s = store ?? await ref.read<Future<RawStore>>(rawStoreProvider.future);
    await s.deleteAll(keep: _writer?.id);
    await storageChanged(store: s);
  }
}
