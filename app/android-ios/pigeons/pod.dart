// Pigeon contract between Flutter and the native pod layer (BLUEPRINT.md section 7).
// Native owns BLE (CoreBluetooth on iOS with state restoration; Nordic Android-BLE-Library in a
// foreground service on Android) so log sync (#20) can run while the app is in the background.
// GATT UUIDs and payloads are in docs/ble_gatt.md (rev 13); Dart parses payloads, native moves bytes.
//
// Regenerate from app/android-ios:  dart run pigeon --input pigeons/pod.dart
import 'package:pigeon/pigeon.dart';

@ConfigurePigeon(
  PigeonOptions(
    dartOut: 'lib/features/pod/data/pod_api.g.dart',
    dartOptions: DartOptions(),
    swiftOut: 'ios/Runner/PodApi.g.swift',
    swiftOptions: SwiftOptions(),
    kotlinOut: 'android/app/src/main/kotlin/open/atrium/PodApi.g.kt',
    kotlinOptions: KotlinOptions(package: 'open.atrium'),
    copyrightHeader: 'pigeons/header.txt',
  ),
)
enum LinkState { idle, scanning, connecting, connected, disconnected, bluetoothOff, unauthorized }

class FoundPod {
  FoundPod({required this.id, required this.name, required this.rssi});

  /// Platform identifier (iOS peripheral UUID, Android MAC).
  String id;
  String name;
  int rssi;
}

class SyncProgress {
  SyncProgress({required this.pagesDone, required this.pagesTotal});
  int pagesDone;
  int pagesTotal;
}

/// Flutter calls native.
@HostApi()
abstract class PodHostApi {
  /// Starts scanning for pods advertising the Atrium service.
  void startScan();
  void stopScan();

  /// Connects and bonds; native keeps the bond for background reconnects.
  @async
  void connect(String id);
  void disconnect();

  /// Id of the bonded pod, if any.
  String? bondedPodId();

  /// Disconnects, stops background reconnects and forgets the stored pod (and on Android removes the bond).
  /// iOS keeps the system pairing until the user forgets it in Settings; the pod accepts a new pairing
  /// over it in the lift window.
  void forgetPod();

  /// Reads a characteristic by its 16-bit id from ble_gatt.md (e.g. 0x0101 Charge State).
  @async
  Uint8List read(int characteristic);

  @async
  void write(int characteristic, Uint8List value, bool withResponse);

  /// Notifications arrive through [PodFlutterApi.onNotify].
  void setNotify(int characteristic, bool enabled);

  /// Downloads the log (service 0x0500) from [fromPage]; pages arrive through [PodFlutterApi.onLogPages].
  @async
  void syncLog(int fromPage);

  /// Asks native to sync in the background at least every [maxHours] hours (16 for #20).
  void setBackgroundSync(bool enabled, int maxHours);
}

/// Native calls Flutter.
@FlutterApi()
abstract class PodFlutterApi {
  void onLinkState(LinkState state);
  void onFound(FoundPod pod);
  void onNotify(int characteristic, Uint8List value);

  /// Whole 4 KB log pages starting at [firstPage]; decoded in Dart (log_record.dart).
  void onLogPages(int firstPage, Uint8List pages);
  void onSyncProgress(SyncProgress progress);
}

/// Readings stay on this phone (#23): keeps the app's data folder out of iCloud and device backups.
/// Android disables backup in the manifest instead, so its side is a no-op.
@HostApi()
abstract class FilesHostApi {
  void keepOnDevice(String path);
}

/// System permissions the pod link needs, asked at the onboarding step that explains them.
/// iOS: CoreBluetooth and UserNotifications ask natively. Android: Bluetooth and notifications are asked from Dart
/// (permission_handler), so those calls just return true; location services (needed for BLE scans before
/// Android 12) are checked here.
@HostApi()
abstract class PermissionHostApi {
  /// iOS: starts Bluetooth, which shows the system prompt the first time; true when Atrium may use it.
  @async
  bool requestBluetooth();

  /// iOS: true when Bluetooth use is already allowed.
  bool hasBluetooth();

  /// iOS: the notification prompt; true when allowed.
  @async
  bool requestNotifications();

  /// Android: true when location services are on (BLE scans need them before Android 12). iOS: always true.
  bool locationServicesOn();

  /// Android: opens the system Location settings. iOS: nothing.
  void openLocationSettings();
}
