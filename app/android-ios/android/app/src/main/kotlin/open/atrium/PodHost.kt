// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

// Native pod link for Android behind the Pigeon PodHostApi (pigeons/pod.dart).
// GATT layout: docs/ble_gatt.md rev 13. Custom UUIDs share the base 8f2aXXXX-4c46-4154-5249-554d2e706f64.
// Android allows one GATT operation at a time, so every read, write and descriptor write is serialized.
// Background sync (#20) moves into a foreground service with a companion-device association next.
package open.atrium

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.ParcelUuid
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

@SuppressLint("MissingPermission") // runtime permissions are requested in Flutter (permission_handler) before any call
class PodHost(private val context: Context, private val events: PodFlutterApi) : PodHostApi {
  private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
  private val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
  private val prefs = context.getSharedPreferences("atrium", Context.MODE_PRIVATE)
  private val ops = Mutex()
  private var gatt: BluetoothGatt? = null
  private val chars = mutableMapOf<Int, BluetoothGattCharacteristic>()
  private var connected: CompletableDeferred<Unit>? = null
  private var pending: CompletableDeferred<ByteArray>? = null
  private var syncDone: CompletableDeferred<Unit>? = null
  private var syncFirst = 0L
  private var syncExpected = 0
  private val syncPages = sortedMapOf<Long, ByteArray>()
  private val syncComplete = mutableSetOf<Long>()
  private var syncTimeout: Job? = null

  companion object {
    private const val PAGE = 4096
    private const val SYNC_IDLE_MS = 10_000L
    private val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    fun uuid(id: Int): UUID = UUID.fromString(String.format("8f2a%04x-4c46-4154-5249-554d2e706f64", id))
    val POWER: UUID = uuid(0x0100)

    /** 16-bit id: SIG characteristics (0000xxxx-0000-1000-8000-00805f9b34fb) or the Atrium base. */
    fun shortId(u: UUID): Int? {
      val s = u.toString().lowercase()
      if (s.endsWith("-0000-1000-8000-00805f9b34fb") && s.startsWith("0000")) return s.substring(4, 8).toInt(16)
      if (s.startsWith("8f2a") && s.endsWith("-4c46-4154-5249-554d2e706f64")) return s.substring(4, 8).toInt(16)
      return null
    }
  }

  private fun emit(state: LinkState) = scope.launch { runCatching { events.onLinkState(state) } }

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
      listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(POWER)).build()),
      ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),
      scanCallback,
    )
  }

  override fun stopScan() {
    manager.adapter?.bluetoothLeScanner?.stopScan(scanCallback)
  }

  // ---- connection ----
  override suspend fun connect(id: String) {
    stopScan()
    val device: BluetoothDevice = manager.adapter.getRemoteDevice(id)
    val done = CompletableDeferred<Unit>()
    connected = done
    emit(LinkState.CONNECTING)
    gatt = if (Build.VERSION.SDK_INT >= 23) device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    else device.connectGatt(context, false, callback)
    done.await()
    if (device.bondState == BluetoothDevice.BOND_NONE) device.createBond()
    prefs.edit().putString("bondedPod", id).apply()
  }

  override fun disconnect() {
    gatt?.disconnect()
  }

  override fun bondedPodId(): String? = prefs.getString("bondedPod", null)

  /** Log out: close the link, forget the stored pod and remove the system bond (hidden API; best effort). */
  override fun forgetPod() {
    val id = prefs.getString("bondedPod", null)
    gatt?.disconnect()
    gatt?.close()
    gatt = null
    chars.clear()
    prefs.edit().remove("bondedPod").remove("backgroundSync").remove("backgroundSyncHours").apply()
    if (id != null) {
      runCatching {
        val device = manager.adapter.getRemoteDevice(id)
        device.javaClass.getMethod("removeBond").invoke(device)
      }
    }
  }

  // ---- GATT operations, one at a time ----
  override suspend fun read(characteristic: Long): ByteArray = ops.withLock {
    val g = gatt ?: throw FlutterError("not_connected", "Pod not connected")
    val c = chars[characteristic.toInt()] ?: throw FlutterError("no_char", "0x%04X not available".format(characteristic))
    val d = CompletableDeferred<ByteArray>()
    pending = d
    if (!g.readCharacteristic(c)) throw FlutterError("read_failed", "Read was not started")
    d.await()
  }

  override suspend fun write(characteristic: Long, value: ByteArray, withResponse: Boolean) {
    ops.withLock {
      val g = gatt ?: throw FlutterError("not_connected", "Pod not connected")
      val c = chars[characteristic.toInt()] ?: throw FlutterError("no_char", "0x%04X not available".format(characteristic))
      val type = if (withResponse) BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT else BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
      val d = CompletableDeferred<ByteArray>()
      pending = d
      if (Build.VERSION.SDK_INT >= 33) g.writeCharacteristic(c, value, type)
      else @Suppress("DEPRECATION") run { c.writeType = type; c.value = value; g.writeCharacteristic(c) }
      d.await()
    }
  }

  override fun setNotify(characteristic: Long, enabled: Boolean) {
    scope.launch { runCatching { enableNotify(characteristic.toInt(), enabled) } }
  }

  private suspend fun enableNotify(id: Int, enabled: Boolean) = ops.withLock {
    val g = gatt ?: return@withLock
    val c = chars[id] ?: return@withLock
    g.setCharacteristicNotification(c, enabled)
    val desc = c.getDescriptor(CCCD) ?: return@withLock
    val indicate = c.properties and BluetoothGattCharacteristic.PROPERTY_INDICATE != 0
    val v = when {
      !enabled -> BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE
      indicate -> BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
      else -> BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
    }
    val d = CompletableDeferred<ByteArray>()
    pending = d
    if (Build.VERSION.SDK_INT >= 33) g.writeDescriptor(desc, v) else @Suppress("DEPRECATION") run { desc.value = v; g.writeDescriptor(desc) }
    d.await()
  }

  /** Log Control 0x0502 op 0x01 (from page, count 0 = to the head); pages stream on Log Data 0x0503. */
  override suspend fun syncLog(fromPage: Long) {
    val done = CompletableDeferred<Unit>()
    syncDone = done
    syncFirst = fromPage
    syncExpected = 0
    syncPages.clear()
    syncComplete.clear()
    enableNotify(0x0502, true)
    enableNotify(0x0503, true)
    val req = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN).put(0x01).putInt(fromPage.toInt()).putInt(0).array()
    armSyncTimeout()
    write(0x0502, req, true)
    done.await()
  }

  override fun setBackgroundSync(enabled: Boolean, maxHours: Long) {
    prefs.edit().putBoolean("backgroundSync", enabled).putLong("backgroundSyncHours", maxHours).apply()
  }

  // ---- callbacks ----
  private val callback = object : BluetoothGattCallback() {
    override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
      if (newState == BluetoothProfile.STATE_CONNECTED) {
        g.requestMtu(247)
      } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
        chars.clear()
        emit(LinkState.DISCONNECTED)
        connected?.completeExceptionally(FlutterError("connect_failed", "Disconnected (status $status)"))
        connected = null
        g.close()
      }
    }

    override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
      g.discoverServices()
    }

    override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
      for (s in g.services) for (c in s.characteristics) shortId(c.uuid)?.let { chars[it] = c }
      emit(LinkState.CONNECTED)
      connected?.complete(Unit)
      connected = null
    }

    @Deprecated("Deprecated in API 33")
    override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
      @Suppress("DEPRECATION") finish(c.value ?: ByteArray(0), status)
    }

    override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
      finish(value, status)
    }

    override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) = finish(ByteArray(0), status)

    override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) = finish(ByteArray(0), status)

    @Deprecated("Deprecated in API 33")
    override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
      @Suppress("DEPRECATION") changed(c, c.value ?: ByteArray(0))
    }

    override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) = changed(c, value)
  }

  private fun finish(value: ByteArray, status: Int) {
    val d = pending ?: return
    pending = null
    if (status == BluetoothGatt.GATT_SUCCESS) d.complete(value) else d.completeExceptionally(FlutterError("gatt_$status", "GATT error $status"))
  }

  private fun changed(c: BluetoothGattCharacteristic, v: ByteArray) {
    val id = shortId(c.uuid) ?: return
    when (id) {
      0x0502 -> logControl(v)
      0x0503 -> logData(v)
      else -> scope.launch { runCatching { events.onNotify(id.toLong(), v) } }
    }
  }

  private fun logControl(v: ByteArray) {
    // Rev 13 end of read: [0x90, reason, u32 whole pages sent], after the last Log Data packet. Reason 1 = the page
    // being sent left the ring. Finish now; the idle timeout stays for disconnects and older firmware.
    if (v.size >= 6 && v[0] == 0x90.toByte()) { finishSync(); return }
    if (v.size < 6 || v[0] != 0x81.toByte()) return
    armSyncTimeout()
    val status = v[1].toInt()
    val count = ByteBuffer.wrap(v, 2, 4).order(ByteOrder.LITTLE_ENDIAN).int
    if (status != 0) { syncTimeout?.cancel(); syncDone?.completeExceptionally(FlutterError("log_status_$status", "Log read refused")); syncDone = null; return }
    syncExpected = count
    if (count == 0) finishSync()
  }

  private fun logData(v: ByteArray) {
    if (v.size <= 6 || syncDone == null) return
    armSyncTimeout()
    val bb = ByteBuffer.wrap(v).order(ByteOrder.LITTLE_ENDIAN)
    val page = bb.int.toLong() and 0xFFFFFFFFL
    val offset = bb.short.toInt() and 0xFFFF
    val buf = syncPages.getOrPut(page) { ByteArray(PAGE) { 0xFF.toByte() } }
    val n = v.size - 6
    if (offset + n <= PAGE) System.arraycopy(v, 6, buf, offset, n)
    if (offset + n >= PAGE) {
      syncComplete.add(page)
      val done = syncComplete.size
      scope.launch { runCatching { events.onSyncProgress(SyncProgress(done.toLong(), syncExpected.toLong())) } }
      if (syncExpected in 1..done) finishSync()
    }
  }

  /** Fallback end of a read (a disconnect, or firmware before rev 13 with no 0x90): after 10 s with no
   *  0x0502/0x0503 traffic, keep the pages that arrived. */
  private fun armSyncTimeout() {
    syncTimeout?.cancel()
    syncTimeout = scope.launch { delay(SYNC_IDLE_MS); finishSync() }
  }

  /** Hands Dart the complete pages, in order, from the first requested page up to the first gap. */
  private fun finishSync() {
    syncTimeout?.cancel()
    syncTimeout = null
    if (syncDone == null) return
    val out = java.io.ByteArrayOutputStream()
    var k = syncFirst
    while (k in syncComplete) { out.write(syncPages[k]!!); k++ }
    val all = out.toByteArray()
    scope.launch { runCatching { events.onLogPages(syncFirst, all) } }
    syncDone?.complete(Unit)
    syncDone = null
  }
}
