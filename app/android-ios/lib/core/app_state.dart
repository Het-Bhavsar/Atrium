// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/storage/local_store.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

enum Build { evt, v1 }

enum DataState { normal, learning, empty }

enum Sky { dawn, day, dusk, night }

enum FirmwareState { available, updating, done }

/// App-wide settings and demo switches. Pod link, sync and status live in `podSessionProvider`, not here.
@immutable
class AppState {
  const AppState({
    this.build = Build.v1,
    this.situation = PodSituation.worn,
    this.data = DataState.normal,
    this.mode = MeasureMode.smart,
    this.sky = Sky.dusk,
    this.themeMode = ThemeMode.system,
    this.glasses,
    this.side = WearSide.left,
    this.padsLeft = 4,
    this.padHours = 31,
    this.onboarded = false,
    this.firmware = FirmwareState.available,
    this.labs = false,
    this.toggles = const {'morning': true, 'bedtime': true, 'charge': true, 'pad': true, 'health': true},
    this.ranges = const {'heart': Range.week, 'sleep': Range.week, 'effort': Range.week, 'temp': Range.month, 'trends': Range.month},
    this.podName,
    this.padChangedAt = 0,
  });

  /// Unix seconds of the last pad change (0 = none yet). The upright calibration restarts there (ML note, J2).
  final int padChangedAt;

  /// Real-data builds: the paired pod's advertised name.
  final String? podName;

  final Build build;
  final PodSituation situation;
  final DataState data;
  final MeasureMode mode;
  final Sky sky;
  final ThemeMode themeMode;
  final bool? glasses;
  final WearSide side;
  final int padsLeft;
  final int padHours;
  final bool onboarded;
  final FirmwareState firmware;
  final bool labs;
  final Map<String, bool> toggles;
  final Map<String, Range> ranges;

  /// The demo pod card. Real-data builds derive it from the pod session (`podStatusProvider`).
  PodStatus get demoPod => PodStatus.demo[situation]!;

  static const PodStatus notConnected = PodStatus(
    label: 'Not nearby',
    batteryPercent: 0,
    onPlinth: false,
    charging: false,
    connected: false,
    line: 'Not connected',
    lastSync: 'not yet',
    hoursHeld: 0,
  );
  bool get isEvt => build == Build.evt;

  AppState copyWith({
    Build? build,
    PodSituation? situation,
    DataState? data,
    MeasureMode? mode,
    Sky? sky,
    ThemeMode? themeMode,
    bool? glasses,
    WearSide? side,
    int? padsLeft,
    int? padHours,
    bool? onboarded,
    FirmwareState? firmware,
    bool? labs,
    Map<String, bool>? toggles,
    Map<String, Range>? ranges,
    String? podName,
    int? padChangedAt,
  }) => AppState(
    build: build ?? this.build,
    situation: situation ?? this.situation,
    data: data ?? this.data,
    mode: mode ?? this.mode,
    sky: sky ?? this.sky,
    themeMode: themeMode ?? this.themeMode,
    glasses: glasses ?? this.glasses,
    side: side ?? this.side,
    padsLeft: padsLeft ?? this.padsLeft,
    padHours: padHours ?? this.padHours,
    onboarded: onboarded ?? this.onboarded,
    firmware: firmware ?? this.firmware,
    labs: labs ?? this.labs,
    toggles: toggles ?? this.toggles,
    ranges: ranges ?? this.ranges,
    podName: podName ?? this.podName,
    padChangedAt: padChangedAt ?? this.padChangedAt,
  );
}

/// The settings kept across launches (Hive box `settings`, `local_store.dart`). Demo switches (build, situation,
/// data state, sky, firmware) are not kept.
extension AppStateStorage on AppState {
  Map<String, Object?> toStored() => {
    'v': 1,
    'mode': mode.name,
    'theme': themeMode.name,
    'glasses': glasses,
    'side': side.name,
    'padsLeft': padsLeft,
    'padHours': padHours,
    'onboarded': onboarded,
    'labs': labs,
    'toggles': toggles,
    'ranges': {for (final e in ranges.entries) e.key: e.value.name},
    'podName': podName,
    'padChangedAt': padChangedAt,
  };

  /// This state with the stored settings over it; anything unreadable keeps its default.
  AppState withStored(Map<Object?, Object?>? m) {
    if (m == null || m['v'] != 1) return this;
    T? pick<T extends Enum>(List<T> values, Object? name) => name is String ? values.asNameMap()[name] : null;
    int? int_(Object? v) => v is int ? v : null;
    bool? bool_(Object? v) => v is bool ? v : null;
    final r = m['ranges'];
    final t = m['toggles'];
    return copyWith(
      mode: pick(MeasureMode.values, m['mode']),
      themeMode: pick(ThemeMode.values, m['theme']),
      glasses: bool_(m['glasses']),
      side: pick(WearSide.values, m['side']),
      padsLeft: int_(m['padsLeft']),
      padHours: int_(m['padHours']),
      onboarded: bool_(m['onboarded']),
      labs: bool_(m['labs']),
      toggles: t is Map ? {...toggles, for (final e in t.entries) '${e.key}': e.value == true} : null,
      ranges: r is Map ? {...ranges, for (final e in r.entries) '${e.key}': pick(Range.values, e.value) ?? Range.week} : null,
      podName: m['podName'] is String ? m['podName']! as String : null,
      padChangedAt: int_(m['padChangedAt']),
    );
  }
}

class AppStateNotifier extends Notifier<AppState> {
  static const _key = 'app';

  @override
  AppState build() {
    final box = ref.watch(settingsBoxProvider);
    return const AppState().withStored(box?.get(_key) as Map<Object?, Object?>?);
  }

  /// Every change goes through here and is kept (when there is storage) if a stored setting moved.
  void _put(AppState next) {
    final before = state.toStored();
    state = next;
    final box = ref.read(settingsBoxProvider);
    final after = next.toStored();
    if (box != null && !_sameMap(before, after)) box.put(_key, after);
  }

  void update(AppState Function(AppState s) f) => _put(f(state));
  void setRange(String key, Range r) => _put(state.copyWith(ranges: {...state.ranges, key: r}));
  void toggle(String key) => _put(state.copyWith(toggles: {...state.toggles, key: !(state.toggles[key] ?? false)}));
  void padChanged() => _put(
    state.copyWith(
      padsLeft: (state.padsLeft - 1).clamp(0, 6),
      padHours: 0,
      side: state.side.other,
      padChangedAt: DateTime.now().millisecondsSinceEpoch ~/ 1000,
    ),
  );
}

bool _sameMap(Map<String, Object?> a, Map<String, Object?> b) {
  if (a.length != b.length) return false;
  for (final k in a.keys) {
    final x = a[k];
    final y = b[k];
    if (x is Map && y is Map) {
      if (x.length != y.length || x.keys.any((key) => x[key] != y[key])) return false;
    } else if (x != y) {
      return false;
    }
  }
  return true;
}

final appStateProvider = NotifierProvider<AppStateNotifier, AppState>(AppStateNotifier.new);
