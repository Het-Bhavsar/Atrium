// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:hive_ce/hive_ce.dart';

/// Local storage on this phone (Hive, `<support dir>/hive`), excluded from backups with the rest of the data folder.
/// Nothing here leaves the phone; "Delete all data" and log out empty it.
///
/// | Box       | Holds                                                                  |
/// |-----------|------------------------------------------------------------------------|
/// | settings  | the app's settings: mode, wear side, pads, reminders, ranges, theme, Labs, onboarding done, pod name |
/// | profile   | the profile (name, age, height, weight)                                  |
/// | metrics   | the last computed metrics, so the next launch opens with numbers         |
///
/// The pod's raw readings stay in `epochs.bin`, 16-byte rows appended as they sync: the exact log codec, read in one
/// pass, and shared with native background sync, which reads and moves the cursor beside it (`spool.dart`). Values are
/// plain maps and strings, so no generated adapters.
abstract final class LocalStore {
  static const String settings = 'settings';
  static const String profile = 'profile';
  static const String metrics = 'metrics';

  static bool _ready = false;

  /// Opens every box; [dir] is the app support folder. Safe to call twice.
  static Future<void> init(String dir) async {
    if (_ready) return;
    Hive.init('$dir${Platform.pathSeparator}hive');
    await Future.wait([Hive.openBox<Object?>(settings), Hive.openBox<Object?>(profile), Hive.openBox<Object?>(metrics)]);
    _ready = true;
  }

  static Box<Object?>? box(String name) => _ready && Hive.isBoxOpen(name) ? Hive.box<Object?>(name) : null;
}

/// The settings box, or null where there is no storage (tests): settings then live in memory only.
final settingsBoxProvider = Provider<Box<Object?>?>((ref) => null);
