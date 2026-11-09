// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import Flutter
import UIKit
import UserNotifications

@main
@objc class AppDelegate: FlutterAppDelegate, FlutterImplicitEngineDelegate {
  private var podHost: PodHost?

  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }

  func didInitializeImplicitFlutterEngine(_ engineBridge: FlutterImplicitEngineBridge) {
    GeneratedPluginRegistrant.register(with: engineBridge.pluginRegistry)
    if let registrar = engineBridge.pluginRegistry.registrar(forPlugin: "AtriumPodHost") {
      let host = PodHost(messenger: registrar.messenger())
      PodHostApiSetup.setUp(binaryMessenger: registrar.messenger(), api: host)
      FilesHostApiSetup.setUp(binaryMessenger: registrar.messenger(), api: FilesHost())
      PermissionHostApiSetup.setUp(binaryMessenger: registrar.messenger(), api: PermissionHost(pod: host))
      podHost = host
    }
  }
}

/// Keeps Atrium's data folder out of iCloud and computer backups: readings stay on this phone (#23).
/// iOS Data Protection already encrypts the files at rest (complete until first unlock, so background sync
/// can still write while the phone is locked).
final class FilesHost: FilesHostApi {
  func keepOnDevice(path: String) throws {
    var url = URL(fileURLWithPath: path, isDirectory: true)
    var values = URLResourceValues()
    values.isExcludedFromBackup = true
    try url.setResourceValues(values)
  }
}

/// Permissions, asked at the onboarding step that explains them: Bluetooth (CoreBluetooth asks when the pod link
/// starts) and notifications (bedtime and morning reminders). Location is an Android matter.
final class PermissionHost: PermissionHostApi {
  private let pod: PodHost
  init(pod: PodHost) { self.pod = pod }

  func requestBluetooth() async throws -> Bool { await pod.requestBluetooth() }

  func hasBluetooth() throws -> Bool { PodHost.bluetoothAllowed }

  func requestNotifications() async throws -> Bool {
    (try? await UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound, .badge])) ?? false
  }

  func locationServicesOn() throws -> Bool { true }

  func openLocationSettings() throws {}

  /// CoreBluetooth wakes Atrium for the pod without Background App Refresh, but a user who turned it off for Atrium
  /// expects nothing in the background, so the app says so.
  func backgroundAllowed() throws -> Bool { UIApplication.shared.backgroundRefreshStatus == .available }

  func openBackgroundSettings() throws {
    if let url = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(url) }
  }
}
