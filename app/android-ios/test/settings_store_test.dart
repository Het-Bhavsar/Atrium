// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/storage/local_store.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:hive_ce/hive_ce.dart';

/// Settings survive a restart through the Hive `settings` box; demo switches don't.
void main() {
  test('settings are kept across launches, demo switches are not', () async {
    final dir = await Directory.systemTemp.createTemp('atrium_settings');
    addTearDown(() async {
      await Hive.close();
      await dir.delete(recursive: true);
    });
    Hive.init(dir.path);
    final box = await Hive.openBox<Object?>('settings');

    final first = ProviderContainer(overrides: [settingsBoxProvider.overrideWithValue(box)]);
    first.read(appStateProvider.notifier)
      ..update(
        (s) => s.copyWith(
          mode: MeasureMode.lite,
          side: WearSide.right,
          onboarded: true,
          labs: true,
          themeMode: ThemeMode.dark,
          podName: 'Atrium 3F2A',
          situation: PodSituation.charging,
        ),
      )
      ..setRange('heart', Range.sixMonths)
      ..toggle('bedtime')
      ..padChanged();
    first.dispose();

    final second = ProviderContainer(overrides: [settingsBoxProvider.overrideWithValue(box)]);
    addTearDown(second.dispose);
    final s = second.read(appStateProvider);
    expect((s.mode, s.side, s.onboarded, s.labs, s.themeMode, s.podName), (MeasureMode.lite, WearSide.left, true, true, ThemeMode.dark, 'Atrium 3F2A'));
    expect(s.ranges['heart'], Range.sixMonths);
    expect(s.toggles['bedtime'], isFalse);
    expect(s.padsLeft, 3, reason: 'one pad used');
    expect(s.padChangedAt, greaterThan(0));
    expect(s.situation, PodSituation.worn, reason: 'demo switches start fresh');
  });

  test('a stored map from another version, or junk, keeps the defaults', () {
    expect(const AppState().withStored({'v': 9, 'mode': 'lite'}).mode, const AppState().mode);
    expect(const AppState().withStored({'v': 1, 'mode': 'nonsense', 'padsLeft': 'x'}).mode, const AppState().mode);
    expect(const AppState().withStored(null).onboarded, isFalse);
  });

  test('no storage: settings live in memory', () {
    final c = ProviderContainer();
    addTearDown(c.dispose);
    c.read(appStateProvider.notifier).update((s) => s.copyWith(labs: true));
    expect(c.read(appStateProvider).labs, isTrue);
    expect(LocalStore.box(LocalStore.settings), isNull);
  });
}
