// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:device_info_plus/device_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';

/// Whether the pod link may scan and connect.
enum BluetoothAccess {
  granted,

  /// The user said no (or took it back in Settings).
  denied,

  /// Android 8-11: permission given, but location services are off, so BLE scans find nothing.
  locationOff,
}

/// Runtime permissions the pod link needs, asked at the onboarding step that explains them.
///
/// Android 12+ (API 31): Nearby devices, i.e. BLUETOOTH_SCAN (never for location) and BLUETOOTH_CONNECT.
/// Android 8-11: a BLE scan needs location permission and location services on.
/// iOS: CoreBluetooth asks when the pod link starts, which `PermissionHostApi.requestBluetooth` does on demand;
/// notifications through UserNotifications.
abstract final class PodPermissions {
  static final _host = PermissionHostApi();

  static Future<bool> _modernAndroid() async => (await DeviceInfoPlugin().androidInfo).version.sdkInt >= 31;

  static Future<List<Permission>> _android() async =>
      await _modernAndroid() ? const [Permission.bluetoothScan, Permission.bluetoothConnect] : const [Permission.locationWhenInUse];

  static Future<BluetoothAccess> _locationCheck() async =>
      await _modernAndroid() || await _host.locationServicesOn() ? BluetoothAccess.granted : BluetoothAccess.locationOff;

  /// The current state, without asking.
  static Future<BluetoothAccess> bluetooth() async {
    if (Platform.isIOS) return await _host.hasBluetooth() ? BluetoothAccess.granted : BluetoothAccess.denied;
    if (!Platform.isAndroid) return BluetoothAccess.granted;
    for (final p in await _android()) {
      if (!await p.isGranted) return BluetoothAccess.denied;
    }
    return _locationCheck();
  }

  /// Asks for what the pod link needs.
  static Future<BluetoothAccess> requestBluetooth() async {
    if (Platform.isIOS) return await _host.requestBluetooth() ? BluetoothAccess.granted : BluetoothAccess.denied;
    if (!Platform.isAndroid) return BluetoothAccess.granted;
    final got = await (await _android()).request();
    if (!got.values.every((s) => s.isGranted)) return BluetoothAccess.denied;
    return _locationCheck();
  }

  /// Bedtime and morning reminders: Android 13+ and iOS ask; older Android grants it at install.
  static Future<bool> requestNotifications() async {
    if (Platform.isIOS) return _host.requestNotifications();
    if (!Platform.isAndroid) return true;
    return (await Permission.notification.request()).isGranted;
  }

  /// The app's page in system Settings, after a refusal.
  static Future<bool> openSettings() => openAppSettings();

  /// Android: the system Location settings page.
  static Future<void> openLocationSettings() => _host.openLocationSettings();

  /// Background sync can run unhindered: Android, Atrium is off battery optimisation (Doze otherwise defers the sync
  /// job to maintenance windows, still well inside the pod's 20 h); iOS, Background App Refresh is on for Atrium.
  static Future<bool> backgroundAllowed() async {
    if (!Platform.isAndroid && !Platform.isIOS) return true;
    try {
      return await _host.backgroundAllowed();
    } on Object {
      return true;
    }
  }

  /// Where the user can allow it: Android's battery optimisation list (no direct exemption request), iOS Settings.
  static Future<void> openBackgroundSettings() => _host.openBackgroundSettings();
}
