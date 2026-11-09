// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/features/profile/data/profile_store.dart';
import 'package:atrium/features/profile/domain/profile.dart';
import 'package:atrium/features/profile/presentation/profile_form.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:hive_ce/hive_ce.dart';

void main() {
  test('profile survives a save and reload, an unreadable entry starts again, and profile.json moves to Hive', () async {
    final dir = await Directory.systemTemp.createTemp('atrium_profile');
    addTearDown(() async {
      await Hive.close();
      await dir.delete(recursive: true);
    });
    Hive.init(dir.path);
    final box = await Hive.openBox<Object?>('profile');
    final store = ProfileStore(box);
    expect(await store.load(), isNull);
    const p = Profile(name: 'Mira', age: 41, heightCm: 170);
    await store.save(p);
    expect(await store.load(), p);
    await box.put('me', '{not a map');
    expect(await store.load(), isNull);

    // An earlier version's file is moved over once, then removed.
    await box.clear();
    final legacy = File('${dir.path}/profile.json')..writeAsStringSync('{"name":"Ravi","age":52}');
    await ProfileStore(box, legacy: legacy).migrateLegacy();
    expect(await store.load(), const Profile(name: 'Ravi', age: 52));
    expect(legacy.existsSync(), isFalse);
  });

  test('summary leaves out what is not set; ranges are checked', () {
    expect(const Profile(name: 'A', age: 30, weightKg: 60).summary, '30 years · 60 kg');
    expect(const Profile(name: 'A').summary, '');
    expect(Profile.checkAge(12), isNotNull);
    expect(Profile.checkHeight(175), isNull);
    expect(Profile.checkWeight(null), isNull);
  });

  testWidgets('the form needs a name and plausible numbers', (tester) async {
    Profile? out = const Profile(name: 'x');
    await tester.pumpWidget(
      MaterialApp(
        theme: buildTheme(Brightness.light),
        home: Scaffold(
          body: ProfileForm(initial: const Profile(), onChanged: (p) => out = p),
        ),
      ),
    );
    await tester.pump();
    expect(out, isNull, reason: 'no name yet');
    final fields = find.byType(TextField);
    await tester.enterText(fields.at(0), 'Mira');
    await tester.pump();
    expect(out, const Profile(name: 'Mira'));
    await tester.enterText(fields.at(1), '9');
    await tester.pump();
    expect(out, isNull, reason: 'age 9 is out of range');
    expect(find.text('Between 13 and 100'), findsOneWidget);
    await tester.enterText(fields.at(1), '41');
    await tester.enterText(fields.at(2), '170');
    await tester.pump();
    expect(out, const Profile(name: 'Mira', age: 41, heightCm: 170));
  });
}
