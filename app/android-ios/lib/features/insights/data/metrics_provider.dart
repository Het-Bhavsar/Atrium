// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/features/insights/data/sample_data.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/pod_metrics.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// The pod link (real-data builds only; null in demo builds).
final podLinkProvider = Provider<PodLink?>((ref) => kRealData ? PodLink() : null);

/// The on-phone epoch store, opened once.
final epochStoreProvider = FutureProvider<EpochStore>((ref) => EpochStore.open());

/// The phone-side sleep model (`assets/models/sleep_v1.json`), loaded once in main; null in tests and demo builds.
final sleepModelsProvider = Provider<SleepModels?>((ref) => null);

/// What every screen reads: demo data, or metrics computed from the synced log.
final metricsProvider = NotifierProvider<MetricsNotifier, MetricsSource>(MetricsNotifier.new);

class MetricsNotifier extends Notifier<MetricsSource> {
  @override
  MetricsSource build() {
    if (!kRealData) return SampleData.instance;
    final side = ref.watch(appStateProvider.select((s) => s.side));
    final padChangedAt = ref.watch(appStateProvider.select((s) => s.padChangedAt));
    final store = ref.watch(epochStoreProvider).value;
    final m = PodMetrics(store?.all() ?? const [], podOnRight: side == WearSide.right, models: ref.watch(sleepModelsProvider), uprightSinceS: padChangedAt);
    // Tell the rest of the app whether there is anything to show yet.
    Future.microtask(() {
      final nights = m.days.where((d) => d.sleepMin > 0).length;
      // Scores need Atrium's formulas (not written yet), so a pod with data shows the learning state.
      ref.read(appStateProvider.notifier).update((s) => s.copyWith(data: m.isEmpty || nights == 0 ? DataState.empty : DataState.learning));
    });
    return m;
  }

  /// Sync the log from the pod, then recompute.
  Future<int> sync() async {
    final link = ref.read(podLinkProvider);
    final store = await ref.read(epochStoreProvider.future);
    if (link == null) return 0;
    final added = await link.sync(store);
    ref.invalidateSelf();
    return added;
  }
}
