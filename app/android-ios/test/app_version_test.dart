// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:atrium/core/config.dart';
import 'package:flutter_test/flutter_test.dart';

/// The version Settings shows is the one the build carries (pubspec `version:` sets Android versionName/versionCode
/// and the iOS bundle version), so a release can't show an old number.
void main() {
  test('Settings shows the pubspec version and build', () {
    final line = File('pubspec.yaml').readAsLinesSync().firstWhere((l) => l.startsWith('version:'));
    expect(line.substring('version:'.length).trim(), '$kAppVersion+$kAppBuild');
  });
}
