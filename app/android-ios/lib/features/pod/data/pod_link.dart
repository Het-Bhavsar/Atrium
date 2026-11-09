// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:typed_data';

import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/foundation.dart' show visibleForTesting;
import 'package:flutter/services.dart';

/// GATT ids used by the app (docs/ble_gatt.md rev 13; the Battery service is gone, SoC is Charge State byte 5). Custom ids share the base
/// 8f2aXXXX-4c46-4154-5249-554d2e706f64; SIG ids are standard.
abstract final class Gatt {
  static const int heartRateMeasurement = 0x2A37;
  static const int firmwareRevision = 0x2A26;
  static const int chargeState = 0x0101;
  static const int ppgControl = 0x0201;
  static const int fitQuality = 0x0206;
  static const int logInfo = 0x0501;

  /// Rev 13: one notification per logged epoch (every 30 s worn or off-body, none on the Plinth).
  static const int liveEpoch = 0x0504;
  static const int mode = 0x0601;
  static const int time = 0x0602;
  static const int sleepWindow = 0x0603;
  static const int command = 0x0605;

  /// Command key 'ATRI'.
  static const int commandKey = 0x41545249;
}

/// Live Epoch 0x0504 (rev 13), 20 bytes: u32 epoch start (Unix, or bit 31 = uptime), u32 head page, then the
/// 12 record bytes exactly as logged.
class LiveEpoch {
  const LiveEpoch(this.epoch, this.headPage);
  final TimedEpoch epoch;
  final int headPage;

  static LiveEpoch? parse(Uint8List v) {
    if (v.length < 8 + recordSize) return null;
    final bd = ByteData.sublistView(v);
    final t = bd.getUint32(0, Endian.little);
    final raw = Uint8List.fromList(v.sublist(8, 8 + recordSize));
    return LiveEpoch(TimedEpoch(t & ~uptimeBit, t & uptimeBit != 0, Epoch.decode(raw), raw), bd.getUint32(4, Endian.little));
  }
}

/// Live pod state parsed from Charge State (0x0101, 19 bytes) and Battery Level (0x2A19).
class LiveStatus {
  const LiveStatus({
    required this.podState,
    required this.chargeState,
    required this.batteryState,
    required this.flags,
    required this.socPercent,
    required this.mode,
  });

  /// 0 worn, 1 docked, 2 power-off, 3 off-body.
  final int podState;

  /// 0 undocked, 1 charging, 2 full, 3 cold-limited, 4 temperature-blocked, 5 charger fault.
  final int chargeState;

  /// 0 ok, 1 low, 2 cut-off.
  final int batteryState;
  final int flags;
  final int socPercent;

  /// Active measurement mode: 0 full, 1 smart, 2 lite, 3 sleep, 4 docked idle, 5 off-body probe.
  final int mode;

  bool get docked => flags & 0x01 != 0;
  bool get onSkin => flags & 0x08 != 0;

  static LiveStatus? parse(Uint8List v) {
    if (v.length < 17) return null;
    return LiveStatus(podState: v[0], chargeState: v[1], batteryState: v[2], flags: v[4], socPercent: v[5], mode: v[16]);
  }

  /// The app's card for this state (battery shown in 10 % steps).
  PodStatus toPodStatus({required String lastSync, required int hoursHeld}) {
    final pct = ((socPercent / 10).round() * 10).clamp(0, 100);
    final cold = chargeState == 3 || chargeState == 4;
    return PodStatus(
      label: docked ? 'On the Plinth' : (onSkin ? 'On skin' : 'Off skin'),
      batteryPercent: pct,
      onPlinth: docked,
      charging: chargeState == 1 || chargeState == 3,
      connected: true,
      offSkin: !docked && !onSkin,
      line: docked
          ? (chargeState == 2
                ? 'On the Plinth, charged'
                : cold
                ? 'On the Plinth, charging paused'
                : 'On the Plinth, charging')
          : onSkin
          ? 'On your temporal region'
          : 'Not on your skin',
      lastSync: lastSync,
      hoursHeld: hoursHeld,
      eta: cold ? 'Paused until the pod warms up' : null,
      alert: cold
          ? PodAlert.cold
          : hoursHeld >= 12
          ? PodAlert.sync
          : null,
    );
  }
}

/// The app's side of the pod link: Pigeon host calls out, Pigeon Flutter API events in.
class PodLink implements PodFlutterApi {
  PodLink({PodHostApi? host}) : _host = host ?? PodHostApi() {
    PodFlutterApi.setUp(this);
  }

  final PodHostApi _host;
  final _state = StreamController<LinkState>.broadcast();
  final _found = StreamController<FoundPod>.broadcast();
  final _notify = StreamController<(int, Uint8List)>.broadcast();
  final _pages = StreamController<(int, Uint8List)>.broadcast();
  final _progress = StreamController<SyncProgress>.broadcast();

  LinkState linkState = LinkState.idle;
  Stream<LinkState> get states => _state.stream;
  Stream<FoundPod> get found => _found.stream;
  Stream<SyncProgress> get syncProgress => _progress.stream;

  /// True while a sync waits for pages; a finished or refused sync leaves it false.
  @visibleForTesting
  bool get waitingForPages => _pages.hasListener;

  @override
  void onLinkState(LinkState state) {
    linkState = state;
    _state.add(state);
  }

  @override
  void onFound(FoundPod pod) => _found.add(pod);

  @override
  void onNotify(int characteristic, Uint8List value) => _notify.add((characteristic, value));

  @override
  void onLogPages(int firstPage, Uint8List pages) => _pages.add((firstPage, pages));

  @override
  void onSyncProgress(SyncProgress progress) => _progress.add(progress);

  Future<void> startScan() => _host.startScan();
  Future<void> stopScan() => _host.stopScan();
  Future<String?> bondedPodId() => _host.bondedPodId();

  /// Connect and bond, then on every connect: read the clock (a pod that rebooted reports uptime, bit 31),
  /// set it (0x0602) so new log blocks carry Unix time, and write the sleep window (0x0603) so night mode
  /// follows local time, time zones and DST. The pod's own default is 23:00-07:00 UTC.
  Future<void> connect(String id, {int sleepStartMin = defaultSleepStart, int sleepEndMin = defaultSleepEnd}) async {
    await _host.connect(id);
    final now = DateTime.now();
    _anchor = null;
    try {
      final before = ByteData.sublistView(await _host.read(Gatt.time)).getUint32(0, Endian.little);
      if (before & uptimeBit != 0) _anchor = (uptime: before & ~uptimeBit, unix: now.millisecondsSinceEpoch ~/ 1000);
    } on PlatformException {
      // Older firmware without a readable clock: uptime epochs are dropped, as before.
    }
    await setClock(now);
    await setSleepWindow(sleepStartMin, sleepEndMin);
  }

  static const int defaultSleepStart = 23 * 60;
  static const int defaultSleepEnd = 7 * 60;

  /// Set when the pod had lost its clock at connect: its uptime then and the phone's Unix time.
  ({int uptime, int unix})? _anchor;

  Future<void> disconnect() => _host.disconnect();

  Future<void> forgetPod() => _host.forgetPod();

  Future<void> setClock([DateTime? now]) {
    final t = (now ?? DateTime.now()).millisecondsSinceEpoch ~/ 1000;
    return _host.write(Gatt.time, _u32(t), true);
  }

  Future<String> firmwareRevision() async => String.fromCharCodes(await _host.read(Gatt.firmwareRevision));

  Future<LiveStatus?> chargeState() async => LiveStatus.parse(await _host.read(Gatt.chargeState));

  /// Charge State notifications (0x0101).
  Stream<LiveStatus> watchChargeState() {
    _host.setNotify(Gatt.chargeState, true);
    return _notify.stream.where((n) => n.$1 == Gatt.chargeState).map((n) => LiveStatus.parse(n.$2)).where((s) => s != null).cast<LiveStatus>();
  }

  /// Mode 0x0601: 0 full, 1 smart, 2 lite.
  Future<void> setMode(MeasureMode m) => _host.write(Gatt.mode, Uint8List.fromList([m.index]), true);

  /// Sleep Window 0x0603: start and end in minutes after local midnight, UTC offset in minutes.
  Future<void> setSleepWindow(int startMin, int endMin) {
    final off = DateTime.now().timeZoneOffset.inMinutes;
    final b = ByteData(6)
      ..setUint16(0, startMin, Endian.little)
      ..setUint16(2, endMin, Endian.little)
      ..setInt16(4, off, Endian.little);
    return _host.write(Gatt.sleepWindow, b.buffer.asUint8List(), true);
  }

  /// Fit guide (#24): PPG Control = 4 (fit mode), then Fit Quality (0x0206) once a second.
  Stream<FitQuality> startFit() {
    _host
      ..write(Gatt.ppgControl, Uint8List.fromList([0x04]), true)
      ..setNotify(Gatt.fitQuality, true);
    return _notify.stream.where((n) => n.$1 == Gatt.fitQuality && n.$2.length >= 2).map((n) => FitQuality(q: n.$2[0], flags: n.$2[1]));
  }

  /// Heart Rate Measurement (SIG 0x2A37): flags, then bpm as u8 (or u16 when flag bit 0 is set).
  Stream<int> heartRate() {
    _host.setNotify(Gatt.heartRateMeasurement, true);
    return _notify.stream
        .where((n) => n.$1 == Gatt.heartRateMeasurement && n.$2.length >= 2)
        .map((n) => n.$2[0] & 1 == 1 && n.$2.length >= 3 ? n.$2[1] | n.$2[2] << 8 : n.$2[1]);
  }

  /// Each new log record as the pod writes it (rev 13). The full sync on connect fills any that were missed.
  Stream<LiveEpoch> liveEpochs() {
    _host.setNotify(Gatt.liveEpoch, true);
    return _notify.stream.where((n) => n.$1 == Gatt.liveEpoch).map((n) => LiveEpoch.parse(n.$2)).where((e) => e != null).cast<LiveEpoch>();
  }

  /// A live epoch whose head page is more than one past the sync cursor means pages were written that no sync
  /// has read (missed notifications, or a long gap): run a sync.
  static bool needsSync({required int headPage, required int cursor}) => headPage > cursor + 1;

  /// Fit mode times out on the pod after 180 s; writing 4 again restarts it.
  Future<void> renewFit() => _host.write(Gatt.ppgControl, Uint8List.fromList([0x04]), true);

  Future<void> stopFit() async {
    await _host.setNotify(Gatt.fitQuality, false);
    await _host.write(Gatt.ppgControl, Uint8List.fromList([0x00]), true);
  }

  /// Ship mode (Command 0x0605 op 0x02): on the Plinth it takes effect at the next lift.
  Future<void> shipMode() {
    final b = ByteData(5)
      ..setUint8(0, 0x02)
      ..setUint32(1, Gatt.commandKey, Endian.little);
    return _host.write(Gatt.command, b.buffer.asUint8List(), true);
  }

  /// Log sync (#20): Log Info 0x0501 gives the oldest and head pages; read from the cursor (or the oldest
  /// page still on the pod) to the head, decode, store. The head page grows, so the next sync reads it again.
  Future<int> sync(EpochStore store) async {
    final info = ByteData.sublistView(await _host.read(Gatt.logInfo));
    final oldest = info.getUint32(0, Endian.little);
    final head = info.getUint32(4, Endian.little);
    final from = syncFrom(cursor: store.syncedToPage, oldest: oldest, head: head);
    // Listen before asking, so pages that arrive with the reply are not missed; cancel on every exit, so a refused
    // read (status 2 or 3) leaves nothing waiting.
    final got = Completer<(int, Uint8List)>();
    final sub = _pages.stream.listen((p) {
      if (!got.isCompleted) got.complete(p);
    });
    final (int, Uint8List) result;
    try {
      await _host.syncLog(from);
      result = await got.future;
    } on PlatformException catch (e) {
      if (e.code == 'log_status_2') return 0; // busy: a sync is already running on the pod
      rethrow;
    } finally {
      await sub.cancel();
    }
    final (first, bytes) = result;
    final anchor = _anchor;
    var epochs = decodeBlocks(bytes);
    if (anchor != null) epochs = rebaseLastRun(epochs, uptimeNow: anchor.uptime, unixNow: anchor.unix);
    final added = await store.add(epochs);
    await store.setCursor(nextCursor(first: first, bytes: bytes.length, cursor: from));
    return added;
  }

  /// After a pod reboot its epochs carry uptime until the phone sets the clock. The last run of those, the
  /// uptime epochs at the end of the log rising without a break up to [uptimeNow], belongs to the current
  /// boot and is moved onto Unix time; uptime epochs from earlier boots can't be placed and stay as they are
  /// (the store drops them).
  static List<TimedEpoch> rebaseLastRun(List<TimedEpoch> epochs, {required int uptimeNow, required int unixNow}) {
    var end = epochs.length - 1;
    while (end >= 0 && !epochs[end].isUptime) {
      end--;
    }
    if (end < 0 || epochs[end].time > uptimeNow) return epochs;
    var start = end;
    while (start > 0 && epochs[start - 1].isUptime && epochs[start - 1].time < epochs[start].time) {
      start--;
    }
    final offset = unixNow - uptimeNow;
    return [
      for (var i = 0; i < epochs.length; i++) i >= start && i <= end ? TimedEpoch(epochs[i].time + offset, false, epochs[i].epoch, epochs[i].raw) : epochs[i],
    ];
  }

  /// Where a sync starts: the cursor while it lies inside the pod's ring, else its oldest page. A cursor past
  /// the head (another or reflashed pod) would otherwise get status 3 on every sync.
  static int syncFrom({required int cursor, required int oldest, required int head}) => cursor >= oldest && cursor <= head ? cursor : oldest;

  /// The next cursor: the last whole page received (the head page grows, so it is read again). A stream the pod
  /// ended early (its ring dropped a page mid-sync) leaves the cursor at the last page that arrived.
  static int nextCursor({required int first, required int bytes, required int cursor}) {
    final n = bytes ~/ pageSize;
    return n == 0 ? cursor : first + n - 1;
  }

  Future<void> setBackgroundSync({bool enabled = true}) => _host.setBackgroundSync(enabled, podLogHours);

  static Uint8List _u32(int v) => (ByteData(4)..setUint32(0, v, Endian.little)).buffer.asUint8List();
}

/// Plain words for pod link failures (rev 12 security): pairing outside the lift window, a link that isn't
/// encrypted yet, a bond the pod no longer matches, or the pod out of reach.
const String bluetoothPermissionText = 'Atrium needs Bluetooth permission to reach the pod. Allow it in Settings, then try again.';

/// Android 8-11 only find Bluetooth devices while Location is on.
const String locationOffText = 'Turn on Location to find the pod. On this version of Android, Bluetooth scans need it; Atrium does not keep your location.';

String podErrorText(Object e) {
  final code = e is PlatformException ? e.code : '';
  final text = '${e is PlatformException ? e.message ?? '' : e}'.toLowerCase();
  if (code == 'SecurityException' || text.contains('permission')) return bluetoothPermissionText;
  if (text.contains('not allowed') || text.contains('pairing not allowed')) {
    return "The pod only pairs in the minute after it's lifted from the Plinth. Lift it off, then try again.";
  }
  if (text.contains('removed pairing') || text.contains('bond')) {
    // The pod replaces this phone's old pairing, but only in the lift window (firmware rev 12, overwrite on).
    return 'This phone was paired with the pod before. Lift the pod from the Plinth, then pair again within a minute.';
  }
  if (text.contains('insufficient') ||
      text.contains('encrypt') ||
      text.contains('authenticat') ||
      code == 'gatt_5' ||
      code == 'gatt_15' ||
      code == 'gatt_137') {
    return "The link to the pod isn't secured yet. Keep it close and accept the pairing request if your phone shows one.";
  }
  if (code == 'not_found' || code == 'not_connected' || text.contains('timed out') || text.contains('timeout')) {
    return "Atrium isn't nearby. Bring it within a few metres and try again.";
  }
  return "Couldn't reach the pod. Lift it off the Plinth, keep it close and try again.";
}
