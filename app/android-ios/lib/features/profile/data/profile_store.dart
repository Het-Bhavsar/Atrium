// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:convert';
import 'dart:io';

import 'package:atrium/core/config.dart';
import 'package:atrium/core/storage/local_store.dart';
import 'package:atrium/features/profile/domain/profile.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:hive_ce/hive_ce.dart';
import 'package:path_provider/path_provider.dart';

/// The profile in the Hive box `profile` (`local_store.dart`). Never leaves the phone. Earlier versions kept it in
/// `profile.json`; the first open moves it over and removes the file.
class ProfileStore {
  ProfileStore(this.box, {this.legacy});
  final Box<Object?> box;
  final File? legacy;
  static const _key = 'me';

  static Future<ProfileStore> open() async {
    final dir = (await getApplicationSupportDirectory()).path;
    await LocalStore.init(dir);
    final store = ProfileStore(LocalStore.box(LocalStore.profile)!, legacy: File('$dir/profile.json'));
    await store.migrateLegacy();
    return store;
  }

  /// Moves an old `profile.json` into the box (if the box has none yet) and removes the file.
  Future<void> migrateLegacy() async {
    final f = legacy;
    if (f == null || !f.existsSync()) return;
    try {
      if (box.get(_key) == null) await box.put(_key, (jsonDecode(await f.readAsString()) as Map).cast<String, Object?>());
    } on Object {
      // Unreadable: start again.
    }
    await f.delete();
  }

  Future<Profile?> load() async {
    try {
      final m = box.get(_key);
      return m is Map ? Profile.fromJson(m.cast<String, Object?>()) : null;
    } on Object {
      return null; // unreadable: start again rather than crash
    }
  }

  Future<void> delete() => box.delete(_key);

  Future<void> save(Profile p) => box.put(_key, p.toJson());
}

/// Set from `main()` with the saved profile; tests and first launch get the default.
final savedProfileProvider = Provider<Profile?>((ref) => null);

/// The store, or null where there is no file system to use (tests).
final profileStoreProvider = Provider<ProfileStore?>((ref) => null);

final profileProvider = NotifierProvider<ProfileNotifier, Profile>(ProfileNotifier.new);

class ProfileNotifier extends Notifier<Profile> {
  @override
  Profile build() => ref.read(savedProfileProvider) ?? (kRealData ? const Profile() : Profile.demo);

  Future<void> save(Profile p) async {
    state = p;
    await ref.read(profileStoreProvider)?.save(p);
  }

  /// Log out: no name on this phone any more.
  Future<void> clear() async {
    state = const Profile();
    await ref.read(profileStoreProvider)?.delete();
  }
}
