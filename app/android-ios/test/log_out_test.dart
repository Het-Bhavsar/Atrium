// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:atrium/features/profile/data/profile_store.dart';
import 'package:atrium/features/profile/domain/profile.dart';
import 'package:atrium/features/settings/data/log_out.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:hive_ce/hive_ce.dart';

/// Atrium has no account: log out must leave nothing of the wearer on this phone.
void main() {
  testWidgets('log out deletes the profile and readings and resets settings but the theme', (tester) async {
    final dir = Directory.systemTemp.createTempSync('atrium_logout');
    addTearDown(() => dir.deleteSync(recursive: true));
    Hive.init(dir.path);
    final box = (await tester.runAsync(() => Hive.openBox<Object?>('profile')))!;
    addTearDown(() => tester.runAsync(Hive.close));
    final profiles = ProfileStore(box);
    await tester.runAsync(() => profiles.save(const Profile(name: 'Mira', age: 41)));
    final epochs = EpochStore(File('${dir.path}/epochs.bin'));
    final raw = Uint8List(12)..[1] = 0x10;
    await tester.runAsync(() => epochs.add([TimedEpoch(1700000000, false, Epoch.decode(raw), raw)]));
    expect(epochs.count, 1);

    late WidgetRef captured;
    await tester.pumpWidget(
      ProviderScope(
        overrides: [
          profileStoreProvider.overrideWithValue(profiles),
          savedProfileProvider.overrideWithValue(const Profile(name: 'Mira', age: 41)),
          epochStoreProvider.overrideWith((ref) async => epochs),
        ],
        child: Consumer(
          builder: (context, ref, _) {
            captured = ref;
            return const SizedBox();
          },
        ),
      ),
    );
    captured.read(appStateProvider.notifier).update((s) => s.copyWith(themeMode: ThemeMode.dark, side: WearSide.right, labs: true));
    expect(captured.read(profileProvider).name, 'Mira');

    await tester.runAsync(() => PhoneData.logOut(captured));

    expect(captured.read(profileProvider).hasName, isFalse);
    expect(box.get('me'), isNull);
    expect(epochs.count, 0);
    expect(File('${dir.path}/epochs.bin').existsSync(), isFalse);
    final s = captured.read(appStateProvider);
    expect(s.themeMode, ThemeMode.dark, reason: 'the theme is a phone preference, kept');
    expect(s.labs, isFalse);
    expect(s.side, const AppState().side);
  });
}
