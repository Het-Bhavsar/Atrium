// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Log sync (#20) as a state machine: one sync at a time, requests during a sync folded into one more run after it,
/// progress in pages, and the outcome kept for the screens. Pure Dart.
///
/// idle -> syncing -> idle (done) | failed;  failed -> syncing
library;

import 'dart:async';

import 'package:flutter/foundation.dart';

enum SyncPhase { idle, syncing, failed }

@immutable
class SyncStatus {
  const SyncStatus({this.phase = SyncPhase.idle, this.pagesDone = 0, this.pagesTotal = 0, this.lastSync, this.lastAdded = 0, this.error});
  final SyncPhase phase;
  final int pagesDone;
  final int pagesTotal;

  /// When the phone last had every reading the pod held.
  final DateTime? lastSync;

  /// Readings the last sync added.
  final int lastAdded;
  final String? error;

  bool get syncing => phase == SyncPhase.syncing;

  /// 0-1 while syncing, when the pod said how many pages it sends.
  double? get progress => pagesTotal > 0 ? (pagesDone / pagesTotal).clamp(0, 1).toDouble() : null;

  SyncStatus copyWith({SyncPhase? phase, int? pagesDone, int? pagesTotal, DateTime? lastSync, int? lastAdded, String? error}) => SyncStatus(
    phase: phase ?? this.phase,
    pagesDone: pagesDone ?? this.pagesDone,
    pagesTotal: pagesTotal ?? this.pagesTotal,
    lastSync: lastSync ?? this.lastSync,
    lastAdded: lastAdded ?? this.lastAdded,
    error: error,
  );
}

/// Runs [task] (a whole sync, returning the readings added) one at a time. A request while one runs returns that run's
/// future and asks for one more run after it, since the pod may have logged more in the meantime; any number of such
/// requests fold into that one. [cancel] stops further runs (the one in flight finishes, its result ignored).
class SyncRunner {
  SyncRunner(this.task, {required this.onChange, DateTime Function()? clock}) : _clock = clock ?? DateTime.now;

  final Future<int> Function() task;
  final void Function(SyncStatus) onChange;
  final DateTime Function() _clock;

  SyncStatus _s = const SyncStatus();
  SyncStatus get status => _s;
  Future<int>? _running;
  bool _again = false;
  bool _cancelled = false;

  /// Runs completed (tests).
  int runs = 0;

  void _set(SyncStatus s) {
    if (_cancelled) return;
    _s = s;
    onChange(s);
  }

  /// Seeds the last sync time (from the store, at launch).
  void restore(DateTime? lastSync) {
    if (lastSync != null && (_s.lastSync == null || lastSync.isAfter(_s.lastSync!))) _set(_s.copyWith(lastSync: lastSync));
  }

  /// Marks readings that arrived live as synced (no run).
  void touched(DateTime at) => _set(_s.copyWith(lastSync: at, error: _s.error));

  void progress(int done, int total) {
    if (_s.syncing) _set(_s.copyWith(pagesDone: done, pagesTotal: total));
  }

  Future<int> request() {
    if (_cancelled) return Future.value(0);
    final r = _running;
    if (r != null) {
      _again = true;
      return r;
    }
    return _running = _loop();
  }

  Future<int> _loop() async {
    var total = 0;
    try {
      do {
        _again = false;
        _set(_s.copyWith(phase: SyncPhase.syncing, pagesDone: 0, pagesTotal: 0));
        try {
          final added = await task();
          runs++;
          total += added;
          _set(_s.copyWith(phase: SyncPhase.idle, lastSync: _clock(), lastAdded: added));
        } on Object catch (e) {
          _set(_s.copyWith(phase: SyncPhase.failed, error: '$e'));
          return total;
        }
      } while (_again && !_cancelled);
      return total;
    } finally {
      _running = null;
    }
  }

  void cancel() {
    _cancelled = true;
    _again = false;
  }

  /// Waits for a run in flight (before handing the link to an update).
  Future<void> idle({Duration timeout = const Duration(seconds: 30)}) async {
    final r = _running;
    if (r == null) return;
    _again = false;
    await r.timeout(timeout, onTimeout: () => 0);
  }
}
