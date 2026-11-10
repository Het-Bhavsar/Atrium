// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Build switches.
///
/// `flutter run --dart-define=ATRIUM_REAL_DATA=true` talks to a real pod over BLE and shows only what the
/// pod has logged. Without it, the app runs on the prototype's demo data (no Bluetooth needed).
library;

/// The maker. Atrium is a Local Forge product; localforge.in is its home on the web.
const String kMaker = 'Local Forge';
const String kMakerSite = 'localforge.in';

const bool kRealData = bool.fromEnvironment('ATRIUM_REAL_DATA');

/// The app's version as Settings shows it; must equal `version:` in pubspec.yaml (`test/app_version_test.dart`).
const String kAppVersion = '0.1.2';
const int kAppBuild = 3;

/// First screen, e.g. `--dart-define=ATRIUM_START=/today` (debugging and screen capture).
const String kStartRoute = String.fromEnvironment('ATRIUM_START', defaultValue: '/welcome');
