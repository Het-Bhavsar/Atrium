// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

// Native pod link for Android behind the Pigeon PodHostApi (pigeons/pod.dart). The GATT client is PodGatt, shared with
// background sync (PodSyncWorker), which runs when the app isn't holding the link.
package open.atrium

import android.annotation.SuppressLint
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

@SuppressLint("MissingPermission") // runtime permissions are requested in Flutter (permission_handler) before any call
class PodHost(private val context: Context, private val events: PodFlutterApi) : PodHostApi {
  private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
  private val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
  private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

  companion object {
    const val PREFS = "atrium"
    const val KEY_POD = "bondedPod"
    const val KEY_BG = "backgroundSync"
    const val KEY_BG_HOURS = "backgroundSyncHours"
    const val KEY_STORE = "storeDir"

    /** True while the app holds the pod link; background sync stays out of the way then. */
    @Volatile var foregroundConnected = false
  }

  private val gatt = PodGatt(context, object : PodGatt.Listener {
    override fun onConnected() { foregroundConnected = true; emit(LinkState.CONNECTED) }
    override fun onDisconnected() { foregroundConnected = false; emit(LinkState.DISCONNECTED) }
    override fun onNotify(id: Int, value: ByteArray) { scope.launch { runCatching { events.onNotify(id.toLong(), value) } } }
    override fun onProgress(done: Int, total: Int) {
      scope.launch { runCatching { events.onSyncProgress(SyncProgress(done.toLong(), total.toLong())) } }
    }
  })

  private fun emit(state: LinkState) = scope.launch { runCatching { events.onLinkState(state) } }

  private inline fun <T> flutter(block: () -> T): T = try { block() } catch (e: PodError) { throw FlutterError(e.code, e.message) }

  // ---- scanning ----
  private val scanCallback = object : ScanCallback() {
    override fun onScanResult(type: Int, r: ScanResult) {
      val name = r.scanRecord?.deviceName ?: "Atrium"
      scope.launch { runCatching { events.onFound(FoundPod(r.device.address, name, r.rssi.toLong())) } }
    }
  }

  override fun startScan() {
    val adapter = manager.adapter
    if (adapter == null || !adapter.isEnabled) { emit(LinkState.BLUETOOTH_OFF); return }
    emit(LinkState.SCANNING)
    adapter.bluetoothLeScanner?.startScan(
      listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(PodGatt.POWER)).build()),
      ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),
      scanCallback,
    )
  }

  override fun stopScan() {
    runCatching { manager.adapter?.bluetoothLeScanner?.stopScan(scanCallback) }
  }

  // ---- connection ----

  /** Connects, bonding the first time and waiting for it, so every read after this is encrypted. The address that
   *  answered is stored: the pod changes its private address every 15 min, and a reconnect may find it by scan. */
  override suspend fun connect(id: String) {
    stopScan()
    emit(LinkState.CONNECTING)
    val address = flutter { gatt.connect(id) }
    prefs.edit().putString(KEY_POD, address).apply()
  }

  /** Also the hand-over to MCUmgr for a pod software update (#21): its own GATT client connects to the same device, so
   *  this one lets go first. */
  override fun disconnect() = gatt.disconnect()

  override fun bondedPodId(): String? = prefs.getString(KEY_POD, null)

  /** Log out: close the link, forget the stored pod, stop background sync and remove the system bond (hidden API;
   *  best effort). */
  override fun forgetPod() {
    val id = prefs.getString(KEY_POD, null)
    gatt.close()
    foregroundConnected = false
    PodSyncWorker.cancel(context)
    prefs.edit().remove(KEY_POD).remove(KEY_BG).remove(KEY_BG_HOURS).apply()
    if (id != null) {
      runCatching {
        val device = manager.adapter.getRemoteDevice(id)
        device.javaClass.getMethod("removeBond").invoke(device)
      }
    }
  }

  override suspend fun read(characteristic: Long): ByteArray = flutter { gatt.read(characteristic.toInt()) }

  override suspend fun write(characteristic: Long, value: ByteArray, withResponse: Boolean) = flutter { gatt.write(characteristic.toInt(), value, withResponse) }

  override fun setNotify(characteristic: Long, enabled: Boolean) {
    scope.launch { runCatching { gatt.enableNotify(characteristic.toInt(), enabled) } }
  }

  /** Downloads the log from [fromPage] and hands Dart the complete pages, decoded there (log_record.dart). */
  override suspend fun syncLog(fromPage: Long) {
    val pages = flutter { gatt.downloadLog(fromPage) }
    runCatching { events.onLogPages(fromPage, pages) }
  }

  /** Background sync (#20): a WorkManager job every [maxHours] h that leaves raw pages in [storeDir]/pod_spool. */
  override fun setBackgroundSync(enabled: Boolean, maxHours: Long, storeDir: String) {
    prefs.edit().putBoolean(KEY_BG, enabled).putLong(KEY_BG_HOURS, maxHours).putString(KEY_STORE, storeDir).apply()
    if (enabled) PodSyncWorker.schedule(context, maxHours) else PodSyncWorker.cancel(context)
  }
}
