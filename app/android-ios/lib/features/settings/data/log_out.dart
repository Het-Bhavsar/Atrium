// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:atrium/features/profile/data/profile_store.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Atrium has no account and no cloud, so these act on this phone only.
abstract final class PhoneData {
  /// Deletes the readings synced to this phone. The pod keeps its own log (about a day) until it rolls over.
  static Future<void> deleteReadings(WidgetRef ref) async {
    final store = await ref.read(epochStoreProvider.future);
    await store.clear();
    ref.invalidate(metricsProvider);
  }

  /// Forgets the paired pod: no reconnects and no background sync.
  static Future<void> forgetPod(WidgetRef ref) async {
    try {
      await ref.read(podLinkProvider)?.forgetPod();
    } on Object {
      // Not paired, or Bluetooth off: nothing to forget.
    }
    ref.read(appStateProvider.notifier).update((s) => AppState(themeMode: s.themeMode, build: s.build, side: s.side, glasses: s.glasses, mode: s.mode));
    ref.invalidate(podSessionProvider);
  }

  /// Log out: forget the pod, delete the readings and the profile, and reset every setting but the theme.
  static Future<void> logOut(WidgetRef ref) async {
    await forgetPod(ref);
    await deleteReadings(ref);
    await ref.read(profileProvider.notifier).clear();
    final theme = ref.read(appStateProvider).themeMode;
    ref.read(appStateProvider.notifier).update((_) => AppState(themeMode: theme));
  }
}
