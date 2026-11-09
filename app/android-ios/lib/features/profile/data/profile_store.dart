import 'dart:convert';
import 'dart:io';

import 'package:atrium/core/config.dart';
import 'package:atrium/features/profile/domain/profile.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';

/// `profile.json` in the app's private folder. Never leaves the phone.
class ProfileStore {
  ProfileStore(this.file);
  final File file;

  static Future<ProfileStore> open() async => ProfileStore(File('${(await getApplicationSupportDirectory()).path}/profile.json'));

  Future<Profile?> load() async {
    try {
      if (!await file.exists()) return null;
      return Profile.fromJson((jsonDecode(await file.readAsString()) as Map).cast<String, Object?>());
    } on Object {
      return null; // unreadable: start again rather than crash
    }
  }

  Future<void> delete() async {
    if (await file.exists()) await file.delete();
  }

  Future<void> save(Profile p) async {
    await file.parent.create(recursive: true);
    await file.writeAsString(jsonEncode(p.toJson()), flush: true);
  }
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
