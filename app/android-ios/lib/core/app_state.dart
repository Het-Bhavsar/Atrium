import 'package:atrium/core/config.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

enum Build { evt, v1 }

enum DataState { normal, learning, empty }

enum Sky { dawn, day, dusk, night }

enum FirmwareState { available, updating, done }

/// App-wide settings and session state. Persisted later through drift; in memory for now.
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
    this.livePod,
    this.podName,
    this.padChangedAt = 0,
  });

  /// Unix seconds of the last pad change (0 = none yet). The upright calibration restarts there (ML note, J2).
  final int padChangedAt;

  /// Real-data builds: the pod's live status (null until connected).
  final PodStatus? livePod;

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

  PodStatus get pod => kRealData ? (livePod ?? notConnected) : PodStatus.demo[situation]!;

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
    PodStatus? livePod,
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
    livePod: livePod ?? this.livePod,
    podName: podName ?? this.podName,
    padChangedAt: padChangedAt ?? this.padChangedAt,
  );
}

class AppStateNotifier extends Notifier<AppState> {
  @override
  AppState build() => const AppState();

  void update(AppState Function(AppState s) f) => state = f(state);
  void setRange(String key, Range r) => state = state.copyWith(ranges: {...state.ranges, key: r});
  void toggle(String key) => state = state.copyWith(toggles: {...state.toggles, key: !(state.toggles[key] ?? false)});
  void padChanged() => state = state.copyWith(
    padsLeft: (state.padsLeft - 1).clamp(0, 6),
    padHours: 0,
    side: state.side.other,
    padChangedAt: DateTime.now().millisecondsSinceEpoch ~/ 1000,
  );
}

final appStateProvider = NotifierProvider<AppStateNotifier, AppState>(AppStateNotifier.new);
