// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/domain/sync_age.dart';
import 'package:atrium/features/pod/presentation/sync_status.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

/// Sync status (#20): the pod's log holds about 20 h, so Today and Device warn once the last sync is over 16 h old.
void main() {
  final now = DateTime(2026, 11, 10, 9);

  test('labels the age of the last sync', () {
    expect(lastSyncedLabel(null, now), 'Not synced yet');
    expect(lastSyncedLabel(now.subtract(const Duration(seconds: 20)), now), 'Just now');
    expect(lastSyncedLabel(now.subtract(const Duration(minutes: 12)), now), '12 min ago');
    expect(lastSyncedLabel(now.subtract(const Duration(hours: 5, minutes: 50)), now), '5 h ago');
    expect(lastSyncedLabel(now.subtract(const Duration(hours: 30)), now), '30 h ago');
    expect(lastSyncedLabel(now.subtract(const Duration(days: 3, hours: 2)), now), '3 days ago');
    expect(lastSyncedSentence(now.subtract(const Duration(hours: 2)), now), 'Last synced 2 h ago');
    expect(lastSyncedSentence(null, now), 'Not synced yet');
  });

  test('warns only after more than 16 h, and never before a first sync', () {
    expect(syncWarnAfter, const Duration(hours: 16));
    expect(syncOverdue(null, now), isFalse);
    expect(syncOverdue(now.subtract(const Duration(hours: 15, minutes: 59)), now), isFalse);
    expect(syncOverdue(now.subtract(const Duration(hours: 16)), now), isFalse);
    expect(syncOverdue(now.subtract(const Duration(hours: 16, minutes: 1)), now), isTrue);
    expect(syncOverdue(now.subtract(const Duration(days: 2)), now), isTrue);
    expect(syncWarnAfter.inHours, lessThan(podLogCapacityHours), reason: 'the warning comes before the log overwrites');
  });

  test('the warning says what happens', () {
    final t = syncOverdueText(now.subtract(const Duration(hours: 17)), now);
    expect(t, contains('Last synced 17 h ago'));
    expect(t, contains('sync soon or older readings will be overwritten'));
  });

  test('hours held never goes negative', () {
    expect(hoursSinceSync(null, now), 0);
    expect(hoursSinceSync(now.add(const Duration(hours: 1)), now), 0);
    expect(hoursSinceSync(now.subtract(const Duration(hours: 7)), now), 7);
  });

  test('the store keeps the last sync time across launches and Delete all data clears it', () async {
    final dir = Directory.systemTemp.createTempSync('atrium_synced');
    addTearDown(() => dir.deleteSync(recursive: true));
    final file = File('${dir.path}/epochs.bin');
    final a = EpochStore(file);
    await a.load();
    expect(a.lastSyncAt, isNull);
    await a.setLastSync(DateTime.fromMillisecondsSinceEpoch(1762765200 * 1000));
    final b = EpochStore(file);
    await b.load();
    expect(b.lastSyncAt!.millisecondsSinceEpoch ~/ 1000, 1762765200);
    await b.clear();
    final c = EpochStore(file);
    await c.load();
    expect(c.lastSyncAt, isNull);
  });

  testWidgets('the notice and the line show on screen', (tester) async {
    final old = now.subtract(const Duration(hours: 18));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildTheme(Brightness.light),
        home: Scaffold(
          body: Column(
            children: [
              LastSyncedLine(lastSync: old, now: now),
              SyncDueNotice(lastSync: old, now: now),
            ],
          ),
        ),
      ),
    );
    expect(find.text('Last synced 18 h ago'), findsOneWidget);
    expect(find.text('Sync soon'), findsOneWidget);
    expect(find.textContaining('older readings will be overwritten'), findsOneWidget);
  });
}
