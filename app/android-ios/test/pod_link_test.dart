// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

/// Log sync cursor rules and Fit Quality values, as the pod firmware defines them (ble_gatt.md rev 11).
void main() {
  test('sync starts at the cursor only while it is inside the ring', () {
    expect(PodLink.syncFrom(cursor: 12, oldest: 10, head: 20), 12);
    expect(PodLink.syncFrom(cursor: 20, oldest: 10, head: 20), 20);
    expect(PodLink.syncFrom(cursor: 4, oldest: 10, head: 20), 10, reason: 'pages before the oldest were overwritten');
    expect(PodLink.syncFrom(cursor: 90, oldest: 0, head: 3), 0, reason: 'a cursor past the head means another or reflashed pod');
  });

  test('the next cursor is the last whole page received', () {
    expect(PodLink.nextCursor(first: 10, bytes: 11 * 4096, cursor: 10), 20);
    expect(PodLink.nextCursor(first: 10, bytes: 3 * 4096, cursor: 10), 12, reason: 'stream ended early');
    expect(PodLink.nextCursor(first: 10, bytes: 0, cursor: 10), 10);
  });

  test('after a reboot, the last uptime run moves onto Unix time; earlier boots stay uptime', () {
    final log = [
      _t(1000), _t(1030), // Unix-stamped, before the reboot
      _t(500, up: true), _t(530, up: true), // an earlier boot
      _t(30, up: true), _t(60, up: true), _t(90, up: true), // this boot
    ];
    final out = PodLink.rebaseLastRun(log, uptimeNow: 100, unixNow: 1700000000);
    expect(out.map((e) => e.isUptime).toList(), [false, false, true, true, false, false, false]);
    expect(out.sublist(4).map((e) => e.time).toList(), [1699999930, 1699999960, 1699999990]);
    expect(out.sublist(0, 4).map((e) => e.time).toList(), [1000, 1030, 500, 530]);
  });

  test('no rebase when the last uptime run is past the clock reading or there is none', () {
    final log = [_t(30, up: true), _t(60, up: true)];
    expect(PodLink.rebaseLastRun(log, uptimeNow: 40, unixNow: 1700000000).every((e) => e.isUptime), isTrue);
    final unix = [_t(1000), _t(1030)];
    expect(PodLink.rebaseLastRun(unix, uptimeNow: 40, unixNow: 1700000000).map((e) => e.time), [1000, 1030]);
  });

  test('a missing Bluetooth permission says so, instead of blaming the pod', () {
    expect(podErrorText(PlatformException(code: 'SecurityException', message: 'Need android.permission.BLUETOOTH_SCAN permission')), bluetoothPermissionText);
  });

  test('link errors read in plain words', () {
    expect(podErrorText(PlatformException(code: 'connect_failed', message: 'Pairing not allowed')), contains('lifted from the Plinth'));
    expect(podErrorText(PlatformException(code: 'gatt_15', message: 'GATT error 15')), contains("isn't secured"));
    expect(podErrorText(PlatformException(code: 'gatt', message: 'Encryption is insufficient.')), contains("isn't secured"));
    expect(podErrorText(PlatformException(code: 'connect_failed', message: 'Peer removed pairing information')), contains('pair again within a minute'));
    expect(podErrorText(PlatformException(code: 'not_found', message: 'Pod x is not nearby')), contains('nearby'));
    expect(podErrorText(Exception('boom')), contains("Couldn't reach"));
  });

  test('q 255 means not measuring yet', () {
    expect(const FitQuality(q: 255, flags: 0).measuring, isFalse);
    expect(const FitQuality(q: 70, flags: 0x30).fitGood, isTrue);
  });
}

TimedEpoch _t(int time, {bool up = false}) => TimedEpoch(
  time,
  up,
  const Epoch(
    pulseBpm: 0,
    pulseConfidence: 0,
    onSkin: false,
    pulseFromIr: false,
    docked: false,
    charging: false,
    rmssdMs: 0,
    cleanDiffs: 0,
    activityClass: ActivityClass.still,
    activityMg: 0,
    mlcLevelMax: 0,
    pulsePresentS: 0,
    skinC: double.nan,
    steps: 0,
    pitchDeg: double.nan,
    rollDeg: double.nan,
    burstR: 0,
    burstConfidence: 0,
  ),
);
