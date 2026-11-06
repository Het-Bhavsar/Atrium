# Atrium pod: BLE GATT interface (rev 9, pod Rev B.2)

This is the contract between the pod firmware (`firmware/src/ble/`) and the app. Atrium is a wellness and research device. Nothing here is a medical measurement.

Feature numbers (#n) refer to [`FEATURES.md`](FEATURES.md).

**Rev 9** follows the r5 Pill board (owner decision): no fuel gauge and no TH3.
- Battery Level and Charge State byte 5 come from the pod's estimate again; byte 6 is VBAT from VBAT_SENSE.
- Charge State bytes 12–15 and 17–18 are reserved with fixed "no reading" values. The length stays 19 bytes, so decoders need no change.
- The charge policy uses TH4 alone.

**Rev 8** adds the motion level and the oxygen trend (PR #11), and the Pill board's 16 Mbit flash:
- Log records grow from 20 to 24 bytes (bytes 20–23: motion level and class, steps/min, pulse-present seconds, oxygen-trend burst). The log holds 11.8 days at 15 s; the phone syncs daily.
- Activity (0x0401) gains max motion level, class and steps/min.
- New Oxygen Ratio (0x0205): the relative red/IR ratio per burst, research only.

**Rev 7** follows the owner's B.2 decision (no heater, MAX17262H fuel gauge):
- Battery Level and Charge State byte 5 come from the gauge's RepSOC. Byte 6 is the gauge's VCell.
- Charge State byte 14 now carries gauge flags (was the heater interlock), byte 15 the gauge die temperature, and new bytes 17–18 the measured average current. Charge State is 19 bytes.
- The NTC roles moved: TH1 is the skin sensor (Skin Temperature, log bytes 6–7, wear detection), and TH3 sits on the board near the cell (Charge State bytes 12–13).

**Rev 6** follows the ML algorithm port:
- PPG Data gains a time stamp (header 8 bytes).
- PPG Control bit 1 starts a raw accelerometer stream on the new Motion Data (0x0204).
- Heart Rate is sent while worn and the spectral track is valid.
- Log byte 15 bit 6 marks an IR pulse source.

**Rev 5** follows the final Rev B.2 netlist:
- The TMP117 and the fuel gauge are gone. Skin and top temperatures come from the NTCs TH3 and TH4, and battery % from the pod's own estimate.
- Charge State byte 14 now reports the heater interlock.
- Modes follow the ML data contract: 25 Hz green, and lite duty-cycled.

**Rev 4** renumbered the modes 0 to 5, moved the PPG stream to the MAXM86161 FIFO format, and put VBAT and head pitch and roll in log bytes 12, 13 and 19 (#5).

## Conventions

- All multi-byte fields are little-endian, except raw PPG FIFO words (see 4.2).
- Temperatures are signed `int16` in 0.01 °C. Voltages are `uint16` in mV.
- `time` is `uint32` Unix seconds once the app has written 6.2. Before that, it is seconds since boot with bit 31 set.
- Security: the pod pairs with LE Secure Connections, Just Works (it has no display or buttons), and bonds. Every characteristic except Device Information and Battery Level needs an encrypted link. Without one, reads and writes fail with Insufficient Encryption (0x0F).
- Default ATT MTU is 23. The pod asks for 247 and uses Data Length Extension. Notifications that carry arrays fill whatever MTU was negotiated.
- Custom UUIDs share one base: `8f2aXXXX-4c46-4154-5249-554d2e706f64`. In the tables below, `0xXXXX` means that base with those 16 bits filled in.

## Advertising

| State | Interval | Payload |
|---|---|---|
| Worn, not connected | 1 s [ASSUMED] | Flags, name `Atrium`, Power service UUID (0x0100) |
| Docked idle, off-body | 2 s | Same as above. Counted in the docked-idle and off-body budgets (`firmware/src/power/modes.c`) |
| Shipping | none | The pod is off |

Connection interval follows the measurement mode: 200 ms (full), 400 ms (smart), 1 s (lite), 4 s (sleep) (`firmware/src/power/modes.c`).

## 1. Device Information (SIG 0x180A): #15

| Characteristic | UUID | Value |
|---|---|---|
| Manufacturer Name | 0x2A29 | `Local Forge` |
| Model Number | 0x2A24 | `Atrium pod` |
| Hardware Revision | 0x2A27 | `B.2` |
| Firmware Revision | 0x2A26 | `<major>.<minor>.<patch>+<build>` |
| Serial Number | 0x2A25 | 16 hex digits of the nRF52832 FICR DEVICEID |

## 2. Battery (SIG 0x180F): #16

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Battery Level | 0x2A19 | R, N | `uint8` %, the pod's estimate (coulomb count from the mode currents, pulled towards the open-circuit voltage at light load; `firmware/src/power/soc_est.c`). Notified on each 1 % change. The 30 mAh cell and a voltage-based estimate are coarse, so the app shows it in 10 % steps. Open without encryption (Zephyr's built-in service) |

## 3. Power and charge (0x0100): #16, #17, #18, #19

### 3.1 Charge State (0x0101): R, N

Notified on any change of the first five bytes, and every 60 s while docked.

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | pod state: 0 worn, 1 docked, 2 shipping, 3 off-body (not worn; measurement paused, #18) |
| 1 | u8 | charge state (charging = CHG# low, or VBAT under the charger's 4.10 V recharge level): 0 undocked, 1 charging, 2 full, 3 cold-limited (0 to 10 °C, ≤ 0.2C average), 4 temperature-blocked (below 0 °C, above 45 °C, or no valid reading) |
| 2 | u8 | battery: 0 ok, 1 low (below 3.45 V loaded; modes drop to lite), 2 cut-off |
| 3 | u8 | temperature band: 0 fault, 1 freeze, 2 cold, 3 normal, 4 hot |
| 4 | u8 | flags: bit 0 docked (VBUS_SENSE), bit 1 CHG_INH held low, bit 2 BQ25101 CHG# low, bit 3 on skin (wear detection, #18: worn and off the dock) |
| 5 | u8 | state of charge, % |
| 6 | u16 | VBAT, mV (loaded, from VBAT_SENSE) |
| 8 | u16 | VBUS, mV (from VBUS_SENSE; 0 when undocked) |
| 10 | i16 | primary cell-temperature proxy, TH4 (top, nearest the cell), 0.01 °C; −32768 = no reading |
| 12 | i16 | reserved: −32768 (was the TH3 backup proxy; r5 has no TH3) |
| 14 | u8 | reserved: 0 (was gauge flags) |
| 15 | i8 | reserved: −128 (was the gauge die temperature) |
| 16 | u8 | active measurement mode: 0 to 3 as in 8, 4 docked idle, 5 off-body probe |
| 17 | i16 | reserved: −32768 (was the gauge's average current) |

**Case battery and "case empty":** both are app-side. The pod cannot see the case battery, and a dead case looks the same as an opened lid: both drop VBUS. The app estimates the case level from the SoC rise while docked × 30 mAh, integrated against the 300 mAh case cell. It shows "charge your case" when the pod reports undocked while it is off-body and still, inside the user's usual dock window (SCHEMATIC_SIM.md 5.4 item 5).

## 4. Pulse: #1, #2

### 4.1 Heart Rate (SIG 0x180D)

The SIG service, so standard fitness apps can read the pulse estimate.

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Heart Rate Measurement | 0x2A37 | N | Flags `0x06` (uint8 format, contact detected), then `uint8` pulse rate in bpm. Sent once a second, only while worn and while the spectral track is valid (confidence ≥ 0.12, `firmware/src/pulse.c`). No notification means no estimate |
| Body Sensor Location | 0x2A38 | R | `0x00` (other: temple) |

### 4.2 PPG stream (0x0200)

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| PPG Control | 0x0201 | W | `u8`: bit 0 raw PPG stream, bit 1 raw accelerometer stream (0 stops both). Streams stop on disconnect |
| PPG Data | 0x0202 | N | Header, then FIFO words (below) |
| Pulse Detail | 0x0203 | R, N | `u8` bpm (0 = none), `u8` confidence of the motion-weighted spectrum (0 to 255 = 0 to 1), `u16` window in s (8), `u8` sample rate in Hz |
| Motion Data | 0x0204 | N | `u16` sequence, `u16` rate in mHz (12500), `u32` uptime ms of the first sample, then up to 12 × (`i16` x, y, z in mg) |
| Oxygen Ratio | 0x0205 | R, N | Relative red/IR ratio, research only (#8): `u32` time, `u16` R × 1000 (0 = no good window), `u8` confidence (0–255 = 0–1), `u8` good windows of 12. Notified at the end of each 30 s burst. No percentage: the app shows the change against the last 14 nights (`app/python/atrium_ml/spo2.py`) |

**PPG Data payload:**

| Offset | Type | Field |
|---|---|---|
| 0 | u16 | sequence number, +1 per notification |
| 2 | u8 | sample rate, Hz (every slot of the mode runs at it) |
| 3 | u8 | measurement mode |
| 4 | u32 | uptime ms of the first sample, the same clock as Motion Data [ASSUMED within one sample] |
| 8 | 3 × n bytes | MAXM86161 FIFO items as read, **big-endian**: bits 23..19 are the tag, bits 18..0 the unsigned, ambient-cancelled value |

Tags follow the MAXM86161 data sheet (Table 3):
- 0x01 to 0x03 are the mode's slots 1 to 3, in the order of `firmware/src/power/modes.c`. LED1 is green, LED2 IR and LED3 red.
- 0x0D to 0x0F are the same slots after picket-fence replacement.
- 0x1D is a sub-DAC update, 0x1E invalid and 0x1F a time stamp.

When the battery is below the green limit, the pod drops the green slot, and the remaining slots move up one place (see `firmware/src/power/headroom.c`). Stream size is `stream_bps` in `modes.c`: 3 bytes × slots × rate. In lite, the stream only flows during the 30 s optics phase.

## 5. Skin temperature (0x0300): #3

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Skin Temperature | 0x0301 | R, N | `i16` skin (TH1, skin side under the window), `i16` top (TH4, under the lid), `u8` flags (bit 0 skin valid, bit 1 top valid), `u32` time. Notified at the mode's NTC rate: 1 s, or 10 s in sleep and docked |

Both sensors sit at the pod, so these are skin-side and pod temperatures, not core body temperature.

## 6. Activity (0x0400): #6 (V1); also an input to #4 and #5

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Activity | 0x0401 | R, N | `u32` time, `u16` activity index (mean of the absolute value of \|a\| − 1 g over the last 60 s, in mg), `u8` flags (bit 0 moving, bit 1 still for ≥ 10 min), then from the last log record: `u8` max motion level (0–7), `u8` class (0 still, 1 walk, 2 run, 3 other), `u8` steps/min (0 = none). Notified once a minute |

## 7. Log download (0x0500): #20

The pod writes one 24-byte record per log interval into the MX25R1635F `log` partition: 1600 KB, 170 records per 4 KB sector, 68 000 records. That is 11.8 days at the default 15 s interval; the phone syncs daily (the 16 Mbit flash of the Pill board cannot hold the 60 days of #20). When the partition is full, the oldest sector is erased first.

**Record (24 bytes):**

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | time |
| 4 | u8 | pulse, bpm (0 = no estimate) |
| 5 | u8 | pulse confidence, 0 to 255 |
| 6 | i16 | skin TH1, 0.01 °C |
| 8 | i16 | top TH4, 0.01 °C |
| 10 | u16 | activity index, mg |
| 12 | u8 | VBAT: 2500 + 10 × value mV (0 = unknown) |
| 13 | i8 | head pitch, degrees, −90 to 90 (−128 = unknown: IMU off) |
| 14 | u8 | SOC, % |
| 15 | u8 | bits 0 to 3 mode (as in 3.1 byte 16), bit 4 docked, bit 5 charging, bit 6 pulse from IR (green had no headroom) |
| 16 | u16 | RMSSD over this interval, 0.1 ms. 0 when there are fewer than 5 clean successive differences |
| 18 | u8 | clean successive differences used (n_i) |
| 19 | i8 | head roll, 2° per step, −90 to 90 = −180° to 180° (−128 = unknown) |
| 20 | u8 | bits 0–2 max MLC motion level in the record, bits 3–5 median level, bits 6–7 class: 0 still, 1 walk, 2 run, 3 other (`firmware/src/mlc.c`) |
| 21 | u8 | step rhythm, steps/min, from the record's last 8 s (0 = none or still). Steps ≈ rhythm × time in walk and run records |
| 22 | u8 | bits 0–3 seconds with a pulse present (0–15); bits 4–7 oxygen-trend burst confidence × 15 (0 = no burst ended in this record) |
| 23 | u8 | that burst's R × 100 (1–255 = 0.01–2.55; 0 = none) |

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Log Info | 0x0501 | R | `u32` oldest sequence, `u32` next sequence, `u16` record size (24), `u16` interval in s, `u32` capacity in records. Free space = capacity − (next − oldest) |
| Log Control | 0x0502 | W, I | Request: `u8` op, then arguments. Response, by indication: `u8` op \| 0x80, `u8` status (0 ok, 1 bad request, 2 busy, 3 out of range), `u32` value |
| Log Data | 0x0503 | N | `u32` sequence of the first record, then whole records |

| Op | Arguments | Effect |
|---|---|---|
| 0x01 read | `u32` from, `u32` count (0 = to the end) | Streams records on Log Data. The response's `value` is the number of records that will be sent |
| 0x02 abort | none | Stops a read |
| 0x03 erase | `u32` up to (exclusive) | Marks records as downloaded. The pod erases them at the next sector boundary |

**HRV (#2).**
- Beats use the max-slope fiducial. An interval is clean when it is within 15 % of the median of the last 5 (`firmware/src/hrv.c`, port of `atrium_ml/hrv.py`).
- Use only records with a low activity index: still, at rest or asleep.
- Overall RMSSD across several records is
√(Σ (RMSSD_i² × n_i) / Σ n_i), where n_i is byte 18. Beats come from the 25 Hz PPG with parabolic timing, so it is a wellness estimate [VERIFY against ECG].

**Summaries are app-side.** Resting pulse, RMSSD, sleep timing and stages, active minutes and mean skin and NTC temperatures are all computed from these records. Each record has a time and a quality value: pulse confidence, and RMSSD with n_i. Steps are not logged [VERIFY: LSM6DSV16X pedometer on a temple-worn pod].

**Resume.** The sequence number is the cursor. To resume, issue Read from the last sequence the app received, plus one.

**Sleep position (#5).**
- Pitch and roll come from the mean accelerometer gravity vector over the interval, in the IMU frame: pitch = atan2(−x, √(y² + z²)), roll = atan2(y, z) [VERIFY axes on the B.2 placement].
- The app classifies back, left, right, front and upright. It calibrates the wear side and pod tilt from a known upright moment.

**Signal check when pairing.** Write 1 to PPG Control, then read Pulse Detail. In smart mode, green is slot 1 (tag 0x01) at 25 Hz.

## 8. Settings (0x0600): #15, #19, #23

Stored in internal flash (Zephyr settings on the `storage` partition), so they survive a reboot. They are lost in ship mode only if the cell runs flat. The cell holds for years in ship mode.

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Mode | 0x0601 | R, W | `u8`: 0 full (green, IR, red), 1 smart (default, green), 2 lite (green, optics 30 s in every 120 s: a pulse every 2 min, no HRV), 3 sleep (green). All at 25 Hz (ML data contract) |
| Time | 0x0602 | R, W | `u32` Unix seconds |
| Sleep Window | 0x0603 | R, W | `u16` start, `u16` end (minutes after local midnight), `i16` UTC offset in minutes. Inside the window the pod uses sleep mode. Default 23:00 to 07:00 (logic_sim.py) |
| Log Interval | 0x0604 | R, W | `u16` seconds, 10 to 600 (default 15) |
| Command | 0x0605 | W | `u8` op, `u32` key `0x41545249`: 0x01 reboot, 0x02 ship now (refused on the dock, error 0x80), 0x03 erase log, 0x04 forget bonds, 0x05 factory reset (log, settings and bonds, then reboot) |

## 9. Firmware update: #21

MCUmgr SMP over BLE. Service `8d53dc1d-1db7-4cd3-868b-8a527460aa84`, characteristic `da2e7828-fbce-4e01-ae9e-261174997c48`. It supports the image and OS groups: upload, test, confirm and reset. It needs an encrypted link.

- MCUboot (swap using move) runs from a 424 KB primary slot in internal flash. The secondary slot of the same size sits at offset 0 of the W25Q64JV.
- A new image confirms itself only after 60 s of healthy running: BLE up, PPG and IMU ready, a VBAT reading. If it is still unhealthy at 300 s, it reboots and MCUboot restores the previous image. An image that was never confirmed also reverts on any reset.
- The images are signed with ECDSA P-256. The development key is MCUboot's sample key [VERIFY: replace it before any release build].
- Updates are refused below 3.60 V VBAT unless the pod is docked [ASSUMED].
- This is the only field update path. Rev B.2 has only VBUS and GND on its pogo pads; SWD is on internal test pads for the bare-board jig.

## 10. Privacy: #23

- No account and no cloud is needed: all of the above works over a bonded link.
- The pod uses resolvable private addresses (`CONFIG_BT_PRIVACY`), so it can't be tracked by its address.
- The data stays in pod flash until the app reads it, and command 0x05 wipes it.
