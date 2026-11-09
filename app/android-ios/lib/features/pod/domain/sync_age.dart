// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// How old the last log sync is (#20). The pod's log holds 8 pages of 4 KB, about 20 h (19.8 to 22.6 h,
/// docs/ble_gatt.md section 7); once it is full the oldest page is erased. The battery lasts days longer, so a pod
/// out of reach for a day loses readings. Pure Dart.
library;

/// The pod's log, in hours, at its shortest without gaps.
const int podLogCapacityHours = 20;

/// Warn once the last sync is older than this: 4 h before the log starts overwriting.
const Duration syncWarnAfter = Duration(hours: 16);

/// True when readings on the pod are at risk: synced before, and more than [syncWarnAfter] ago.
bool syncOverdue(DateTime? lastSync, DateTime now) => lastSync != null && now.difference(lastSync) > syncWarnAfter;

/// Whole hours since the last sync (0 when never synced, or the clock went back).
int hoursSinceSync(DateTime? lastSync, DateTime now) => lastSync == null ? 0 : now.difference(lastSync).inHours.clamp(0, 1 << 30);

/// "Just now", "12 min ago", "5 h ago", "3 days ago", or "Not synced yet".
String lastSyncedLabel(DateTime? lastSync, DateTime now) {
  if (lastSync == null) return 'Not synced yet';
  final d = now.difference(lastSync);
  if (d.inMinutes < 1) return 'Just now';
  if (d.inMinutes < 60) return '${d.inMinutes} min ago';
  if (d.inHours < 48) return '${d.inHours} h ago';
  return '${d.inDays} days ago';
}

/// "Last synced 5 h ago", or "Not synced yet".
String lastSyncedSentence(DateTime? lastSync, DateTime now) =>
    lastSync == null ? 'Not synced yet' : 'Last synced ${lastSyncedLabel(lastSync, now).toLowerCase()}';

/// The warning's text, with the age in it.
String syncOverdueText(DateTime lastSync, DateTime now) =>
    'Last synced ${lastSyncedLabel(lastSync, now).toLowerCase()}. The pod keeps about $podLogCapacityHours hours of readings: sync soon or older readings will be overwritten. Bring your phone near the pod and open Atrium.';
