// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

// One GATT client for the pod, shared by the app's link (PodHost) and background sync (PodSyncWorker).
// GATT layout: docs/ble_gatt.md rev 13. Custom UUIDs share the base 8f2aXXXX-4c46-4154-5249-554d2e706f64.
// Android allows one GATT operation at a time, so every read, write and descriptor write is serialized.
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
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.ParcelUuid
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout

/** A failure with the code Dart maps to plain words (`podErrorText`). PodHost passes it on as a FlutterError. */
class PodError(val code: String, override val message: String) : Exception(message)

@SuppressLint("MissingPermission") // runtime permissions are requested in Flutter before any call; the worker checks
class PodGatt(private val context: Context, private val listener: Listener) {
  interface Listener {
    fun onConnected() {}
    fun onDisconnected() {}
    fun onNotify(id: Int, value: ByteArray) {}
    fun onProgress(done: Int, total: Int) {}
  }

  private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
  private val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
  private val ops = Mutex()
  private var gatt: BluetoothGatt? = null
  private val chars = mutableMapOf<Int, BluetoothGattCharacteristic>()
  private var ready: CompletableDeferred<Unit>? = null
  private var pending: CompletableDeferred<ByteArray>? = null
  private var syncDone: CompletableDeferred<Unit>? = null
  private var syncExpected = 0
  private val syncPages = sortedMapOf<Long, ByteArray>()
  private val syncComplete = mutableSetOf<Long>()
  private var syncTimeout: Job? = null

  val isConnected: Boolean get() = gatt != null && chars.isNotEmpty()

  companion object {
    const val PAGE = 4096
    private const val SYNC_IDLE_MS = 10_000L
    private const val CONNECT_MS = 12_000L
    private const val SCAN_MS = 8_000L
    private const val BOND_MS = 30_000L
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

    fun le32(v: Long): ByteArray = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v.toInt()).array()
    fun u32(b: ByteArray, at: Int = 0): Long = ByteBuffer.wrap(b, at, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL
  }

  // ---- connection ----

  /** Connects to [address] and bonds the first time; returns the address that answered. The pod uses resolvable
   *  private addresses (a new one every 15 min), so when a bonded pod doesn't answer at the stored address, a short
   *  scan finds it by its service and Android's bond (the stack resolves the address for a bonded device). */
  suspend fun connect(address: String): String {
    val adapter = manager.adapter ?: throw PodError("bluetooth_off", "Bluetooth is not available")
    if (!adapter.isEnabled) throw PodError("bluetooth_off", "Bluetooth is off")
    val device = adapter.getRemoteDevice(address)
    try {
      connectTo(device)
      return device.address
    } catch (e: Exception) {
      if (device.bondState != BluetoothDevice.BOND_BONDED) throw e
      val found = scanForBonded() ?: throw e
      connectTo(found)
      return found.address
    }
  }

  private suspend fun connectTo(device: BluetoothDevice) {
    close()
    val done = CompletableDeferred<Unit>()
    ready = done
    gatt = if (Build.VERSION.SDK_INT >= 23) device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    else device.connectGatt(context, false, callback)
    try {
      withTimeout(CONNECT_MS) { done.await() }
    } catch (e: TimeoutCancellationException) {
      close()
      throw PodError("not_found", "Connect timed out")
    }
    if (device.bondState != BluetoothDevice.BOND_BONDED) bond(device)
  }

  /** Bonds and waits for the result: every characteristic but Device Information needs an encrypted link, and the pod
   *  drops a link still unencrypted after 30 s. Outside its lift window the pod refuses (0x05): "not allowed". */
  private suspend fun bond(device: BluetoothDevice) {
    val result = CompletableDeferred<Int>()
    val receiver = object : BroadcastReceiver() {
      override fun onReceive(c: Context, i: Intent) {
        val d: BluetoothDevice? = if (Build.VERSION.SDK_INT >= 33) i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
        else @Suppress("DEPRECATION") i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
        if (d?.address != device.address) return
        val s = i.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.BOND_NONE)
        val prev = i.getIntExtra(BluetoothDevice.EXTRA_PREVIOUS_BOND_STATE, BluetoothDevice.BOND_NONE)
        if (s == BluetoothDevice.BOND_BONDED || (s == BluetoothDevice.BOND_NONE && prev == BluetoothDevice.BOND_BONDING)) result.complete(s)
      }
    }
    val filter = IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
    if (Build.VERSION.SDK_INT >= 33) context.registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED)
    else context.registerReceiver(receiver, filter)
    try {
      if (device.bondState == BluetoothDevice.BOND_BONDED) return
      if (device.bondState != BluetoothDevice.BOND_BONDING && !device.createBond()) throw PodError("bond_failed", "Pairing not allowed")
      val s = try { withTimeout(BOND_MS) { result.await() } } catch (e: TimeoutCancellationException) { BluetoothDevice.BOND_NONE }
      if (s != BluetoothDevice.BOND_BONDED) {
        close()
        throw PodError("bond_failed", "Pairing not allowed")
      }
    } finally {
      runCatching { context.unregisterReceiver(receiver) }
    }
  }

  /** A short, filtered, low-latency scan for a pod this phone is bonded with. */
  private suspend fun scanForBonded(): BluetoothDevice? {
    val scanner = manager.adapter?.bluetoothLeScanner ?: return null
    val found = CompletableDeferred<BluetoothDevice>()
    val cb = object : ScanCallback() {
      override fun onScanResult(type: Int, r: ScanResult) {
        if (r.device.bondState == BluetoothDevice.BOND_BONDED) found.complete(r.device)
      }
    }
    scanner.startScan(
      listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(POWER)).build()),
      ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),
      cb,
    )
    return try { withTimeout(SCAN_MS) { found.await() } } catch (e: TimeoutCancellationException) { null } finally { runCatching { scanner.stopScan(cb) } }
  }

  /** Lets the link go. A connect still pending is cancelled and closed, since Android may not call back for it. */
  fun disconnect() {
    val waiting = ready
    if (waiting != null) {
      ready = null
      close()
      waiting.completeExceptionally(PodError("cancelled", "Connect cancelled"))
      listener.onDisconnected()
      return
    }
    gatt?.disconnect()
  }

  fun close() {
    gatt?.disconnect()
    gatt?.close()
    gatt = null
    chars.clear()
  }

  fun dispose() {
    close()
    scope.cancel()
  }

  // ---- GATT operations, one at a time ----

  suspend fun read(id: Int): ByteArray = ops.withLock {
    val g = gatt ?: throw PodError("not_connected", "Pod not connected")
    val c = chars[id] ?: throw PodError("no_char", "0x%04X not available".format(id))
    val d = CompletableDeferred<ByteArray>()
    pending = d
    if (!g.readCharacteristic(c)) { pending = null; throw PodError("read_failed", "Read was not started") }
    d.await()
  }

  suspend fun write(id: Int, value: ByteArray, withResponse: Boolean) {
    ops.withLock {
      val g = gatt ?: throw PodError("not_connected", "Pod not connected")
      val c = chars[id] ?: throw PodError("no_char", "0x%04X not available".format(id))
      val type = if (withResponse) BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT else BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
      val d = CompletableDeferred<ByteArray>()
      pending = d
      if (Build.VERSION.SDK_INT >= 33) g.writeCharacteristic(c, value, type)
      else @Suppress("DEPRECATION") run { c.writeType = type; c.value = value; g.writeCharacteristic(c) }
      d.await()
    }
  }

  suspend fun enableNotify(id: Int, enabled: Boolean) = ops.withLock {
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

  /** Log Control 0x0502 op 0x01 (from page, count 0 = to the head); pages stream on Log Data 0x0503. Returns the
   *  complete pages, in order, from [fromPage] up to the first gap. The link runs at high priority (short connection
   *  interval) for the download only, then goes back to balanced to spare both batteries. */
  suspend fun downloadLog(fromPage: Long): ByteArray {
    val done = CompletableDeferred<Unit>()
    syncDone = done
    syncExpected = 0
    syncPages.clear()
    syncComplete.clear()
    gatt?.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
    try {
      enableNotify(0x0502, true)
      enableNotify(0x0503, true)
      val req = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN).put(0x01).putInt(fromPage.toInt()).putInt(0).array()
      armSyncTimeout()
      write(0x0502, req, true)
      done.await()
    } finally {
      gatt?.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_BALANCED)
    }
    val out = ByteArrayOutputStream()
    var k = fromPage
    while (k in syncComplete) { out.write(syncPages[k]!!); k++ }
    return out.toByteArray()
  }

  // ---- callbacks ----
  private val callback = object : BluetoothGattCallback() {
    override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
      if (newState == BluetoothProfile.STATE_CONNECTED) {
        g.requestMtu(247)
      } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
        chars.clear()
        ready?.completeExceptionally(PodError("connect_failed", "Disconnected (status $status)"))
        ready = null
        // A GATT operation waiting on a dropped link never gets its callback: fail it, so the next one can run.
        pending?.completeExceptionally(PodError("not_connected", "Pod disconnected"))
        pending = null
        if (syncDone != null) finishSync()
        g.close()
        if (gatt === g) gatt = null
        listener.onDisconnected()
      }
    }

    override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
      // 2M PHY where both ends have it: the log and live data move in half the air time.
      if (Build.VERSION.SDK_INT >= 26) g.setPreferredPhy(BluetoothDevice.PHY_LE_2M_MASK, BluetoothDevice.PHY_LE_2M_MASK, BluetoothDevice.PHY_OPTION_NO_PREFERRED)
      g.discoverServices()
    }

    override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
      for (s in g.services) for (c in s.characteristics) shortId(c.uuid)?.let { chars[it] = c }
      listener.onConnected()
      ready?.complete(Unit)
      ready = null
    }

    @Deprecated("Deprecated in API 33")
    override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
      @Suppress("DEPRECATION") finish(c.value ?: ByteArray(0), status)
    }

    override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) = finish(value, status)

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
    if (status == BluetoothGatt.GATT_SUCCESS) d.complete(value) else d.completeExceptionally(PodError("gatt_$status", "GATT error $status"))
  }

  private fun changed(c: BluetoothGattCharacteristic, v: ByteArray) {
    val id = shortId(c.uuid) ?: return
    when (id) {
      0x0502 -> logControl(v)
      0x0503 -> logData(v)
      else -> listener.onNotify(id, v)
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
    if (status != 0) {
      syncTimeout?.cancel()
      syncDone?.completeExceptionally(PodError("log_status_$status", "Log read refused"))
      syncDone = null
      return
    }
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
      listener.onProgress(syncComplete.size, syncExpected)
      if (syncExpected in 1..syncComplete.size) finishSync()
    }
  }

  /** Fallback end of a read (a disconnect, or firmware before rev 13 with no 0x90): after 10 s with no
   *  0x0502/0x0503 traffic, keep the pages that arrived. */
  private fun armSyncTimeout() {
    syncTimeout?.cancel()
    syncTimeout = scope.launch { delay(SYNC_IDLE_MS); finishSync() }
  }

  private fun finishSync() {
    syncTimeout?.cancel()
    syncTimeout = null
    syncDone?.complete(Unit)
    syncDone = null
  }
}
