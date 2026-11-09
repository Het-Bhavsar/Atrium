// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Which on-phone models run, picked by what the phone can do. Pure Dart.
///
/// What the phone runs (ML note 2026-11-08): the pod already computes pulse, pulse quality (confidence, fit) and
/// per-epoch RMSSD; the phone only pools them per night. The phone's own models are in `assets/models/sleep_v1.json`:
/// - sleep / wake (#4), a 7 x 2 logistic model over a night of epochs: every phone runs it;
/// - sleep stages (Labs), a 7 x 3 logistic model: only with Labs on, and not on low-end phones, where it would
///   double the nightly work for a research estimate (kappa 0.28).
/// There is nothing heavier in the app: no research model is bundled, so none can run by accident.
///
/// Every tier computes off the UI isolate; the tier sets how often live readings trigger a recompute and how many
/// days are recomputed.
library;

enum DeviceTier { low, mid, high }

/// Low: under 3 GB of RAM, or Android's low-RAM flag; mid: under 6 GB; high: the rest.
DeviceTier tierFor({required int ramMb, bool lowRamDevice = false}) {
  if (lowRamDevice || (ramMb > 0 && ramMb < 3000)) return DeviceTier.low;
  if (ramMb > 0 && ramMb < 6000) return DeviceTier.mid;
  return ramMb <= 0 ? DeviceTier.mid : DeviceTier.high;
}

class ModelPlan {
  const ModelPlan({required this.tier, required this.sleepStages, required this.liveRefresh, required this.historyDays});

  final DeviceTier tier;

  /// Sleep / wake always runs; this is the Labs stage model.
  final bool sleepStages;

  /// Live readings recompute the screens at most this often (a full sync always does).
  final Duration liveRefresh;

  /// Days recomputed from the log: the six-month range plus a margin for the baselines.
  final int historyDays;

  bool get sleepWake => true;

  @override
  String toString() => 'ModelPlan(${tier.name}, stages: $sleepStages)';
}

ModelPlan planFor(DeviceTier tier, {required bool labs}) => switch (tier) {
  DeviceTier.low => const ModelPlan(tier: DeviceTier.low, sleepStages: false, liveRefresh: Duration(minutes: 15), historyDays: 200),
  DeviceTier.mid => ModelPlan(tier: DeviceTier.mid, sleepStages: labs, liveRefresh: const Duration(minutes: 5), historyDays: 200),
  DeviceTier.high => ModelPlan(tier: DeviceTier.high, sleepStages: labs, liveRefresh: const Duration(minutes: 2), historyDays: 200),
};
