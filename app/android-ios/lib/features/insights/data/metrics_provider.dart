// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:convert';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/storage/local_store.dart';
import 'package:atrium/features/insights/data/sample_data.dart';
import 'package:atrium/features/insights/domain/metrics_snapshot.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/model_plan.dart';
import 'package:atrium/features/insights/domain/pod_metrics.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:atrium/features/profile/data/profile_store.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// The pod link (real-data builds only; null in demo builds).
final podLinkProvider = Provider<PodLink?>((ref) => kRealData ? PodLink() : null);

/// The on-phone epoch store, opened once.
final epochStoreProvider = FutureProvider<EpochStore>((ref) => EpochStore.open());

/// The phone-side sleep model (`assets/models/sleep_v1.json`), loaded once in main; null in tests and demo builds.
final sleepModelsProvider = Provider<SleepModels?>((ref) => null);

/// What this phone can run, probed once in main (`device_tier.dart`); mid until then and in tests.
final deviceTierProvider = Provider<DeviceTier>((ref) => DeviceTier.mid);

/// Which on-phone models run: the tier, and Labs for the stage model.
final modelPlanProvider = Provider<ModelPlan>((ref) => planFor(ref.watch(deviceTierProvider), labs: ref.watch(appStateProvider.select((s) => s.labs))));

/// The metrics saved at the end of the last compute, read in main so the first frame has numbers; null when none.
final metricsCacheProvider = Provider<MetricsSnapshot?>((ref) => null);

/// What every screen reads: demo data, or metrics computed from the synced log.
final metricsProvider = NotifierProvider<MetricsNotifier, MetricsSource>(MetricsNotifier.new);

/// Up to this many readings (about two days) the metrics are computed in place; above it, on another isolate.
const int inlineEpochs = 6000;

class MetricsNotifier extends Notifier<MetricsSource> {
  int _generation = 0;

  @override
  MetricsSource build() {
    if (!kRealData) return SampleData.instance;
    final side = ref.watch(appStateProvider.select((s) => s.side));
    final padChangedAt = ref.watch(appStateProvider.select((s) => s.padChangedAt));
    final plan = ref.watch(modelPlanProvider);
    final models = ref.watch(sleepModelsProvider);
    final age = ref.watch(profileProvider.select((p) => p.age));
    final store = ref.watch(epochStoreProvider).value;
    final shown = stateOrNull ?? ref.read(metricsCacheProvider) ?? MetricsSnapshot.empty(DateTime.now());
    if (store == null) return shown;
    final job = MetricsJob(store.all(), podOnRight: side == WearSide.right, models: models, uprightSinceS: padChangedAt, plan: plan, age: age);
    final gen = ++_generation;
    if (job.epochs.length <= inlineEpochs) {
      final m = computeMetrics(job);
      Future.microtask(() => _computed(m, gen));
      return m;
    }
    // Months of readings: the screens keep what they show (the saved snapshot at launch) until this lands.
    unawaited(compute(computeMetrics, job).then((m) => _computed(m, gen)));
    return shown;
  }

  void _computed(MetricsSnapshot m, int gen) {
    if (gen != _generation) return;
    state = m;
    final nights = m.days.where((d) => d.sleepMin > 0).length;
    // Recovery needs five nights for a baseline (scores.dart); until then the screens show the learning state.
    final data = m.isEmpty || nights == 0
        ? DataState.empty
        : m.recoveryParts?.score == null
        ? DataState.learning
        : DataState.normal;
    ref.read(appStateProvider.notifier).update((s) => s.copyWith(data: data));
    unawaited(MetricsCache.save(m));
  }

  /// Sync the log from the pod, then recompute.
  Future<int> sync() async {
    final link = ref.read(podLinkProvider);
    final store = await ref.read(epochStoreProvider.future);
    if (link == null) return 0;
    final added = await link.sync(store);
    if (added > 0) ref.invalidateSelf();
    return added;
  }
}

/// Everything one compute needs, as plain data so it can cross to another isolate.
class MetricsJob {
  MetricsJob(this.epochs, {required this.podOnRight, required this.models, required this.uprightSinceS, required this.plan, this.age});
  final List<TimedEpoch> epochs;
  final bool podOnRight;
  final SleepModels? models;
  final int uprightSinceS;
  final ModelPlan plan;
  final int? age;
}

MetricsSnapshot computeMetrics(MetricsJob j) => MetricsSnapshot.of(
  PodMetrics(
    j.epochs,
    podOnRight: j.podOnRight,
    models: j.models,
    uprightSinceS: j.uprightSinceS,
    stages: j.plan.sleepStages,
    historyDays: j.plan.historyDays,
    age: j.age,
  ),
);

/// The saved snapshot, in the Hive box `metrics` (`local_store.dart`). Emptied with "Delete all data" (it holds the
/// same readings).
abstract final class MetricsCache {
  static const _key = 'snapshot';

  static Future<MetricsSnapshot?> load() async {
    try {
      final s = LocalStore.box(LocalStore.metrics)?.get(_key);
      return s is String ? MetricsSnapshot.fromJson(jsonDecode(s)) : null;
    } on Object {
      return null;
    }
  }

  static Future<void> save(MetricsSnapshot m) async {
    try {
      await LocalStore.box(LocalStore.metrics)?.put(_key, jsonEncode(m.toJson()));
    } on Object {
      // Next compute saves it.
    }
  }

  static Future<void> clear() async => LocalStore.box(LocalStore.metrics)?.delete(_key);
}
