// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

package open.atrium

import android.content.Context
import android.content.Intent
import android.location.LocationManager
import android.os.Build
import android.provider.Settings
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine

class MainActivity : FlutterActivity() {
  override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
    super.configureFlutterEngine(flutterEngine)
    val messenger = flutterEngine.dartExecutor.binaryMessenger
    PodHostApi.setUp(messenger, PodHost(applicationContext, PodFlutterApi(messenger)))
    FilesHostApi.setUp(messenger, object : FilesHostApi {
      // Backup is off in the manifest (allowBackup, dataExtractionRules), so nothing leaves the phone.
      override fun keepOnDevice(path: String) {}
    })
    PermissionHostApi.setUp(messenger, PermissionHost(this))
  }
}

/** Bluetooth and notification permissions are asked from Dart (permission_handler); this side checks that
 *  location services are on, which BLE scans need before Android 12, and opens their settings page. */
class PermissionHost(private val activity: FlutterActivity) : PermissionHostApi {
  override suspend fun requestBluetooth(): Boolean = true

  override fun hasBluetooth(): Boolean = true

  override suspend fun requestNotifications(): Boolean = true

  override fun locationServicesOn(): Boolean {
    val lm = activity.getSystemService(Context.LOCATION_SERVICE) as LocationManager
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
      lm.isLocationEnabled
    } else {
      @Suppress("DEPRECATION")
      Settings.Secure.getInt(activity.contentResolver, Settings.Secure.LOCATION_MODE, Settings.Secure.LOCATION_MODE_OFF) !=
        Settings.Secure.LOCATION_MODE_OFF
    }
  }

  override fun openLocationSettings() {
    activity.startActivity(Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS))
  }
}
