// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

// Background log sync (#20) on Android: a periodic WorkManager job that reaches the bonded pod while the app is closed,
// downloads the log the same way the app does and leaves the raw pages for the app (lib/features/pod/data/spool.dart).
// The pod keeps about 20 h; the job runs every few hours, so a phone left alone still keeps every reading.
package open.atrium

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ForegroundInfo
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.TimeUnit
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout

class PodSyncWorker(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {
  companion object {
    private const val NAME = "atrium-pod-sync"
    private const val CHANNEL = "pod-sync"
    private const val NOTIFICATION = 21
    private const val SPOOL_MAGIC = 0x31505341L

    fun schedule(context: Context, hours: Long) {
      val req = PeriodicWorkRequestBuilder<PodSyncWorker>(hours.coerceAtLeast(1), TimeUnit.HOURS).build()
      WorkManager.getInstance(context).enqueueUniquePeriodicWork(NAME, ExistingPeriodicWorkPolicy.UPDATE, req)
    }

    fun cancel(context: Context) {
      WorkManager.getInstance(context).cancelUniqueWork(NAME)
    }

    /** The spool header (spool.dart): magic, first page, pod clock at connect, phone Unix time at connect. */
    fun spool(first: Long, podClock: Long, phoneUnix: Long, pages: ByteArray): ByteArray =
      ByteBuffer.allocate(16 + pages.size).order(ByteOrder.LITTLE_ENDIAN)
        .putInt(SPOOL_MAGIC.toInt()).putInt(first.toInt()).putInt(podClock.toInt()).putInt(phoneUnix.toInt()).put(pages).array()
  }

  override suspend fun doWork(): Result {
    val prefs = applicationContext.getSharedPreferences(PodHost.PREFS, Context.MODE_PRIVATE)
    if (!prefs.getBoolean(PodHost.KEY_BG, false)) return Result.success()
    val id = prefs.getString(PodHost.KEY_POD, null) ?: return Result.success()
    val dir = prefs.getString(PodHost.KEY_STORE, null) ?: return Result.success()
    // The app holds the link and syncs live: nothing to do.
    if (PodHost.foregroundConnected) return Result.success()
    if (Build.VERSION.SDK_INT >= 31 &&
      applicationContext.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED
    ) return Result.success()
    // A short foreground service (type connectedDevice) where the system allows one; the sync takes seconds either way.
    runCatching { setForeground(foregroundInfo()) }

    val gatt = PodGatt(applicationContext, object : PodGatt.Listener {})
    return try {
      withTimeout(120_000) {
        val address = gatt.connect(id)
        if (address != id) prefs.edit().putString(PodHost.KEY_POD, address).apply()
        // The pod's clock before setting it: bit 31 means it rebooted and logs uptime, which the app rebases.
        val podClock = runCatching { PodGatt.u32(gatt.read(0x0602)) }.getOrDefault(0L)
        val now = System.currentTimeMillis() / 1000
        gatt.write(0x0602, PodGatt.le32(now), true)
        val info = gatt.read(0x0501)
        val oldest = PodGatt.u32(info, 0)
        val head = PodGatt.u32(info, 4)
        val cursorFile = File(dir, "epochs.bin.cursor")
        val cursor = runCatching { cursorFile.readText().trim().toLong() }.getOrDefault(0L)
        val from = if (cursor in oldest..head) cursor else oldest
        val pages = gatt.downloadLog(from)
        val whole = pages.size / PodGatt.PAGE
        if (whole > 0) {
          withContext(Dispatchers.IO) {
            val spoolDir = File(dir, "pod_spool").apply { mkdirs() }
            val tmp = File(spoolDir, "$now-$from.tmp")
            tmp.writeBytes(spool(from, podClock, now, pages.copyOf(whole * PodGatt.PAGE)))
            tmp.renameTo(File(spoolDir, "$now-$from.bin"))
            cursorFile.writeText("${from + whole - 1}")
          }
        }
      }
      Result.success()
    } catch (e: Exception) {
      // Out of reach or refused: WorkManager tries again with back-off, and the next period comes anyway.
      Result.retry()
    } finally {
      gatt.dispose()
    }
  }

  override suspend fun getForegroundInfo(): ForegroundInfo = foregroundInfo()

  private fun foregroundInfo(): ForegroundInfo {
    val nm = applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
    if (Build.VERSION.SDK_INT >= 26 && nm.getNotificationChannel(CHANNEL) == null) {
      nm.createNotificationChannel(NotificationChannel(CHANNEL, "Pod sync", NotificationManager.IMPORTANCE_MIN))
    }
    val n = NotificationCompat.Builder(applicationContext, CHANNEL)
      .setContentTitle("Copying readings from your pod")
      .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
      .setPriority(NotificationCompat.PRIORITY_MIN)
      .setOngoing(true)
      .build()
    return if (Build.VERSION.SDK_INT >= 29) ForegroundInfo(NOTIFICATION, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
    else ForegroundInfo(NOTIFICATION, n)
  }
}
