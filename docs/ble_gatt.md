# Atrium pod: BLE GATT interface (rev 16, pod EVT)

This is the contract between the pod firmware (`firmware/src/ble/`) and the app. Atrium is a wellness and research device. Nothing here is a medical measurement.

Feature numbers (#n) refer to [`FEATURES.md`](FEATURES.md).

**Rev 16** adds two research-only vibration modes for a bench study of bone-conducted voice and jaw vibration; nothing earlier changes, so a rev 15 app still works:
- Mode (0x0601) takes 10 (accelerometer at 3.33 kHz) and 11 (6.66 kHz). The LEDs and the gyroscope are off. Like modes 6 to 9 they are not stored, last one connection and never run on the Plinth (§4.4).
- New Vibration Data (0x0207, N): one accelerometer axis, the one normal to the skin, as raw 16-bit samples. Every notification has a sequence number, the uptime of its first sample and that sample's index, so gaps show.
- A bench rig (§4.5): the same packets, mode 11, gap-free over SPI on an nRF52 DK, as text lines on USB serial. It is not part of the pod and changes nothing over the air.

**Rev 15** adds one flag for the charging case ([charging-case.md](charging-case.md)); nothing earlier changes, so a rev 14 app still works:
- Charge State byte 4 bit 4: fed by the charging case. The case marks its VIN with two 200 ms drop-outs about 3 s after it starts feeding (and after a reset); the Plinth does not, so the bit tells them apart (2026-11-08). It clears when VIN goes away for more than 400 ms, so a full pod resting in the case with VIN off reads undocked [ASSUMED: the case switches VIN off once the pod is full].
- Three such drop-outs within 2 s reboot the pod (the case's soft reset). The phone sees a disconnect.

**Rev 14** adds four research-only modes for the depth study (experiment E1); nothing earlier changes, so a rev 13 app still works:
- Mode (0x0601) takes 6 (research, 100 Hz) and 7 (research, 200 Hz): green, IR and red plus a direct-ambient slot, with the accelerometer at 104 Hz (§4.3). None of them is stored: they last one connection, and never run on the Plinth.
- Modes 8 and 9 sample the same slots at 200 Hz and average them on the module to 50 Hz and 25 Hz (SMP_AVE 4 and 8). The mode byte marks averaged output; the rate byte is the output rate.
- The direct-ambient slot streams on PPG Data with tag 0x04.
- Motion Data's rate field carries rates above 32.767 Hz with bit 15 set, in 10 mHz steps (104 Hz = 0xA8A0). 12.5 Hz is still 12500.

**Rev 13** completes the log sync (#20); nothing earlier changes, so a rev 12 app still works:
- New Live Epoch (0x0504, N): each 12-byte record the moment it is logged, with its time and the head page, so a connected phone gets every 30 s epoch without a sync.
- A read ends with an end indication on Log Control: `0x90`, reason, whole pages sent. It comes after the last Log Data packet; the phone no longer waits for its idle timeout when a read ends early.
- A read sends the head page as it stood at the request. A record logged during the read is in the next sync, never half in this one (before, a record could arrive with its "written" byte set and its first bytes 0xFF).
- While a read runs the pod asks for a 15–30 ms connection interval, then goes back to the mode's.

**Rev 12** hardens the link (Conventions: Security; section 10):
- Pairing is LE Secure Connections only, with 16-byte keys. A link that is not LESC is dropped, and so is one still not encrypted 30 s after it connects.
- A phone that lost its bond pairs again in the lift window; the new bond replaces the old one.
- No Battery service (it was open without encryption): read SoC from Charge State byte 5. No Serial Number in Device Information (a fixed identifier).

**Rev 11** adds the fit guide (#24):
- New Fit Quality (0x0206): the live 0–100 fit score, with its flags, perfusion and regularity.
- PPG Control bit 2 is fit mode: green at 25 Hz before wear detection says worn.
- Heart Rate (0x2A37) also notifies in fit mode before wear says worn, but only with a pulse present.

**Rev 10** follows the EVT baseline (2026-11-07): BQ21061 charger on I²C, VSYS sense, no external flash.
- Charge State: byte 6 is VSYS (the cell voltage off the Plinth, 4.5 V on it); byte 8 is reserved (no VBUS sense); flag bits 1 and 2 mean charging disabled and cold current; bytes 14–15 carry the BQ21061 STAT0 and STAT1; charge state 5 is a charger fault; pod state 2 is power-off.
- The log is one 12-byte record per 30 s epoch, in page-packed blocks, 8 pages of internal flash. Log Info and Log Data address 4 KB pages, and the erase-up-to op is gone.
- Log Interval is read-only: 30 s.
- Command 0x02 works on and off the Plinth.
- New pairings only in the 60 s after a lift from the Plinth.

Revs 4 to 9 described earlier boards and log formats (a fuel gauge, external flash, a third NTC). They no longer apply; what still holds from them (the PPG Data header, Motion Data, Oxygen Ratio and Activity's motion fields) is in the sections below.

## Conventions

- All multi-byte fields are little-endian, except raw PPG FIFO words (see 4.2).
- Temperatures are signed `int16` in 0.01 °C. Voltages are `uint16` in mV.
- `time` is `uint32` Unix seconds once the app has written 6.2. Before that, it is seconds since boot with bit 31 set.
- Security (`firmware/src/ble/ble.c`, `ble_policy.h`):
  - The pod pairs with LE Secure Connections only (`CONFIG_BT_SMP_SC_PAIR_ONLY`), Just Works, since it has no display or buttons. It bonds, with 16-byte keys. That is Zephyr security level 2. Levels 3 and 4 need MITM protection, which a passkey would give.
  - It accepts a new pairing only in the 60 s after a lift from the Plinth, and in the 60 s after boot. Outside that, pairing fails with Pairing Not Allowed (0x05). Bonded phones reconnect at any time.
  - A link with a legacy bond or a short key is dropped (HCI 0x05, authentication failure). So is a link still not encrypted 30 s after it connects, so a stranger can't hold the pod's one connection.
  - Every characteristic except Device Information (four strings and the PnP ID) and Zephyr's GAP and GATT services needs an encrypted link. Without one, a bonded phone gets Insufficient Encryption (0x0F) and an unbonded one Insufficient Authentication (0x05) [VERIFY on a phone]. CCC descriptors read without encryption; writing them needs it.
  - A phone that has lost its bond (forgotten in its settings, reinstalled) pairs again the same way: lift the pod from the Plinth, then pair within 60 s. The new bond replaces its old one (`CONFIG_BT_SMP_ALLOW_UNAUTH_OVERWRITE`, 2026-11-08). Outside the window the pod refuses with 0x05, so replacing a bond needs the pod in hand. A build without the window check doesn't compile (`BUILD_ASSERT` in `ble.c`).
  - At most 4 bonds are held, and the oldest is not overwritten: a fifth phone fails to pair until command 0x04 (forget bonds) or 0x05 (factory reset) from a bonded phone.
- Default ATT MTU is 23. The pod accepts up to 247 when the phone asks; it has no GATT client, so it cannot start the exchange. It asks for Data Length Extension and 2M PHY only in modes 10 and 11 (§4.4). Notifications that carry arrays fill whatever MTU was negotiated.
- Custom UUIDs share one base: `8f2aXXXX-4c46-4154-5249-554d2e706f64`. In the tables below, `0xXXXX` means that base with those 16 bits filled in.

## Advertising

| State | Interval | Payload |
|---|---|---|
| Not connected, any mode except docked idle (off-body included) | 1 s [ASSUMED] | Flags and the Power service UUID (0x0100); name `Atrium` in the scan response |
| Docked idle (on the Plinth) | 2 s | Same as above. Counted in the docked-idle budget (`firmware/src/power/modes.c`) |
| Power-off (cut-off), ship mode | none | The pod is off until the next Plinth |

Connection interval follows the mode, with latency 0: 200 ms (full), 400 ms (smart), 1 s (lite), 4 s (sleep), 2 s (docked idle and off-body), 30 ms (modes 6 and 7), 100 ms (modes 8 and 9), 15 ms (modes 10 and 11) (`firmware/src/power/modes.c`). A log read asks for 15–30 ms instead (§7).

## 1. Device Information (SIG 0x180A): #15

| Characteristic | UUID | Value |
|---|---|---|
| Manufacturer Name | 0x2A29 | `Local Forge` |
| Model Number | 0x2A24 | `Atrium pod` |
| Hardware Revision | 0x2A27 | `EVT` |
| Firmware Revision | 0x2A26 | `<major>.<minor>.<patch>+<commit>`, e.g. `0.1.0+41f1d65` (`-dirty` for an uncommitted build). Tag every recording with it: the board's IMU rotation is tied to the commit |
| PnP ID | 0x2A50 | Zephyr's defaults: vendor ID source 1, vendor 0, product 0, version 1 |

Device Information is open without encryption (Zephyr's built-in service), so it carries no serial number or other per-unit identifier.

## 2. Battery: #16

No Battery service: Zephyr's is open without encryption. The pod's SoC estimate is Charge State (0x0101) byte 5 (`firmware/src/power/soc_est.c`). It is notified when it changes. The 30 mAh cell and a voltage-based estimate are coarse, so the app shows it in 10 % steps.

## 3. Power and charge (0x0100): #16, #17, #18, #19

### 3.1 Charge State (0x0101): R, N

Notified on any change of bytes 0–5 (states, flags and SoC), and every 60 s while docked.

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | pod state: 0 worn, 1 docked, 2 power-off (cut-off reached), 3 off-body (not worn; measurement paused, #18) |
| 1 | u8 | charge state: 0 undocked, 1 charging, 2 full (BQ21061 charge done), 3 cold-limited (0 to 10 °C on TH4: 5 mA), 4 temperature-blocked (TH4 below 0 °C, above 45 °C, or no valid reading; or the charger's own TS window), 5 charger fault (no I²C answer, VIN over-voltage or battery over-current) |
| 2 | u8 | battery: 0 ok, 1 low (below 3.45 V loaded; modes drop to lite), 2 cut-off |
| 3 | u8 | temperature band (TH4): 0 fault, 1 freeze, 2 cold, 3 normal, 4 hot |
| 4 | u8 | flags: bit 0 docked (/PG low), bit 1 charging disabled by firmware, bit 2 cold current set, bit 3 on skin (wear detection, #18: worn and off the dock), bit 4 fed by the charging case (its VIN signature seen; docked only) |
| 5 | u8 | state of charge, % |
| 6 | u16 | VSYS, mV: the loaded cell voltage off the Plinth; about 4.5 V (regulated) on it |
| 8 | u16 | reserved: 0 (no VBUS sense) |
| 10 | i16 | firmware charge-temperature sensor, TH4 (top, next to the nRF), 0.01 °C; −32768 = no reading |
| 12 | i16 | reserved: −32768 (no backup sensor) |
| 14 | u8 | BQ21061 STAT0 at the last charger service (every 10 s on the Plinth); 0 off it |
| 15 | u8 | BQ21061 STAT1, the same way |
| 16 | u8 | active measurement mode: 0 to 3 as in 8, 4 docked idle, 5 off-body probe, 6 to 9 research (§4.3), 10 and 11 research vibration (§4.4) |
| 17 | i16 | reserved: −32768 |

**Case battery and "case empty":** both are app-side. The pod cannot see the case battery, and a dead case looks the same as a lift: both release /PG. The app estimates the case level from the SoC rise while fed by the case (flag bit 4) × 30 mAh, integrated against the 500 mAh case cell (KP-502030; about 8 pod charges, `docs/charging-case.md`). It shows "charge your case" when the pod reports undocked while it is off-body and still, inside the user's usual dock window.

## 4. Pulse: #1, #2

### 4.1 Heart Rate (SIG 0x180D)

The SIG service, so standard fitness apps can read the pulse estimate.

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Heart Rate Measurement | 0x2A37 | N | Flags `0x06` (uint8 format, contact detected), then `uint8` pulse rate in bpm. Sent once a second while the spectral track is valid (confidence ≥ 0.12, `firmware/src/pulse.c`) and the pod is worn. In fit mode (§4.2) it is also sent before wear says worn, with a pulse present (`pulse_report_bpm`). The log records only the worn rate. No notification means no estimate |
| Body Sensor Location | 0x2A38 | R | `0x01` (chest): Zephyr's fixed value. The firmware does not set it, so ignore it; the pod sits on the temporal region |
| Heart Rate Control Point | 0x2A39 | W | Present in Zephyr's service; the pod registers no handler, so writes fail |

### 4.2 PPG stream (0x0200)

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| PPG Control | 0x0201 | W | `u8`: bit 0 raw PPG stream, bit 1 raw accelerometer stream, bit 2 fit mode (0 stops all). Streams and fit mode stop on disconnect; fit mode also after 180 s |
| PPG Data | 0x0202 | N | Header, then FIFO words (below) |
| Pulse Detail | 0x0203 | R, N | `u8` bpm (0 = none), `u8` confidence of the motion-weighted spectrum (0 to 255 = 0 to 1), `u16` window in s (8), `u8` sample rate in Hz |
| Motion Data | 0x0204 | N | `u16` sequence, `u16` rate, `u32` uptime ms of the first sample, then 12 × (`i16` x, y, z in mg), 80 bytes, sent only with an ATT MTU of 83 or more. None in docked idle, off-body or modes 10 and 11. Rate: bit 15 clear, mHz (12500 = 12.5 Hz); bit 15 set, bits 0–14 in 10 mHz (0xA8A0 = 104 Hz, research modes) (`gatt_motion_rate`) |
| Oxygen Ratio | 0x0205 | R, N | Relative red/IR ratio, research only (#8): `u32` time, `u16` R × 1000 (0 = no good window), `u8` confidence (0–255 = 0–1), `u8` good windows of 12. Notified at the end of each 30 s burst. No percentage: the app shows the change against the last 14 nights |
| Fit Quality | 0x0206 | R, N | Fit guide (#24), below. Notified once a second while subscribed, off the Plinth |
| Vibration Data | 0x0207 | N | Research modes 10 and 11 only: the accelerometer's skin-normal axis at 3.33 or 6.66 kHz (§4.4) |

**PPG Data payload:**

| Offset | Type | Field |
|---|---|---|
| 0 | u16 | sequence number, +1 per notification |
| 2 | u8 | sample rate, Hz (every slot of the mode runs at it) |
| 3 | u8 | measurement mode |
| 4 | u32 | uptime ms of the first sample, the same clock as Motion Data [ASSUMED within one sample] |
| 8 | 3 × n bytes | MAXM86161A FIFO items as read, **big-endian**: bits 23..19 are the tag, bits 18..0 the unsigned value: ambient-cancelled for a LED slot, the raw photodiode level for the direct-ambient slot |

Tags follow the MAXM86161A data sheet (Table 3):
- 0x01 to 0x04 are the mode's slots 1 to 4, in the order of `firmware/src/power/modes.c`. LED1 is green, LED2 IR and LED3 red. Slot 4 exists only in the research modes: 0x04 is direct ambient (§4.3).
- 0x0D to 0x0F are slots 1 to 3 after picket-fence replacement. The data sheet has no picket-fence tag for slot 4.
- 0x1D is a sub-DAC update, 0x1E invalid and 0x1F a time stamp.

When VSYS is below the green headroom limit (`firmware/src/power/headroom.c`), green becomes IR at the same current in the same slot (smart, lite, sleep, the probe). In full, which already has IR, the green slot is dropped and IR and red move up one place (`mode_green_to_ir` in `modes.c`). During an oxygen-trend burst (30 s, smart and sleep only), IR (tag 0x02) and red (tag 0x03) join green in the stream. Stream size is `stream_bps` in `modes.c`: 3 bytes × slots × rate. In lite, the stream only flows during the 30 s optics phase. A research mode never drops green: below the limit the pod runs full instead, and the header's mode byte says 0.

### 4.3 Research modes (depth study)

Research only: for the depth study's ambient and timing questions (E1), never a default and not a wellness feature. Every other mode takes one integration per 25 Hz sample (SMP_AVE 1), so pulse content above 12.5 Hz aliases. On public finger data the 95th percentile of per-beat error is 20 ms for pulse arrival and 8.8 ms for IR–green lag; mode 7 gives 5 ms and 1.9 ms, and mode 8 gives 5 ms and 2.1 ms. None of the other modes has an ambient slot.

| | Mode 6 | Mode 7 | Mode 8 | Mode 9 |
|---|---|---|---|---|
| Slots, tags | green 15 mA (0x01), IR 10 mA (0x02), red 10 mA (0x03), direct ambient (0x04) | same | same | same |
| Module sample rate (PPG_SR) | 100 Hz | 200 Hz | 200 Hz | 200 Hz |
| Averaged on the module (SMP_AVE) | none | none | 4 | 8 |
| PPG Data rate (header byte 2) | 100 Hz | 200 Hz | 50 Hz | 25 Hz |
| Accelerometer | 104 Hz, rate field 0xA8A0 | same | same | same |
| PPG Data | 1200 B/s of FIFO words + 100 B/s of headers (12.5 notifications/s of 32 words) | 2400 + 200 B/s (25/s) | 600 + 50 B/s (6.25/s) | 300 + 25 B/s (3.1/s) |
| Motion Data | 624 B/s of samples + 69 B/s of headers (8.7 notifications/s) | same | same | same |
| Connection interval | 30 ms | 30 ms | 100 ms | 100 ms |
| LED current at the cell [ASSUMED] | 524 µA | 1049 µA | 1049 µA: LEDs fire at 200 Hz, as in mode 7 | 1049 µA, as in mode 7 |
| Module (4 exposures per sample) [ASSUMED], typ / worst | 533 / 800 µA | 1067 / 1600 µA | 1067 / 1600 µA, as in mode 7 | 1067 / 1600 µA, as in mode 7 |
| Radio, CPU, I²C, IMU and the rest [ASSUMED], typ / worst | 455 / 492 µA | 590 / 637 µA | 327 / 361 µA: less BLE data, 100 ms interval | 292 / 324 µA |
| Total at the cell [ASSUMED], typ / worst | 1512 / 1816 µA | 2706 / 3286 µA | 2443 / 3010 µA | 2408 / 2973 µA |

- Select: write `u8` 6, 7, 8 or 9 to Mode (0x0601) off the Plinth, then 3 to PPG Control for both streams. On the Plinth the write fails with Value Not Allowed (0x13). Mode reads back the value while it runs.
- Ends: on disconnect, on the Plinth, at reboot, or when 0 to 3 is written. None of these is stored, so the stored mode comes back.
- The usual rules still apply. On the Plinth the pod is docked idle and off the body it runs the probe. In the sleep window it runs sleep and on a low battery lite. Below the green headroom limit (`firmware/src/power/headroom.c`) it runs full, so the slot table is always the one above.
- **Direct ambient (DA):** LEDC code 0x9 (MAXM86161A data sheet, Table 2). The photodiode is sampled with no LED, after the three LED exposures, and the module does not cancel ambient for it. Its tag is its slot, 0x04, as in the data sheet's FIFO example. Decode: `tag 4 → "ambient"` for modes 6 to 9.
- **Averaged output (modes 8 and 9).** The PPG Data header's mode byte is the flag: 8 and 9 are averaged on the module, and every other mode is one integration per sample. Byte 2 is the output rate.
  - Each FIFO word is the mean of 4 (mode 8) or 8 (mode 9) adjacent 200 Hz samples of its own slot. The module averages each channel separately (data sheet, SMP_AVE), so it averages DA too [VERIFY on the bench].
  - The 4 exposures take 4 × 129.8 µs (tINT 117.3 µs + settling 12 µs + 0.5 µs) per 5 ms frame. The data sheet allows 400 sps for 4 exposures at this tINT.
  - Timing: a word stands for the centre of its window, (avg − 1) / 2 samples before its last one. That is 7.5 ms (mode 8) or 17.5 ms (mode 9), the same for every slot, so it cancels in IR–green lag. The header time stamp follows the other modes [ASSUMED within one output sample].
  - Current [ASSUMED]: averaging does not cut the optics. The LEDs and the module still run at 200 Hz, so their current is mode 7's (1049 and 1067 µA typ); only the BLE data and the radio's share drop, by 263 µA (mode 8) or 298 µA (mode 9) typ. Against full at 25 Hz (437 µA typ), modes 8 and 9 draw about 5.5 times as much.
  - Burst mode would idle between bursts, but its rates are 8, 32, 84 and 256 Hz, so it gives no 25 or 50 Hz output.
- On the pod, the pulse, HRV and fit filters still run at 25 Hz: each takes the mean of 4 or 8 green samples (2 in mode 8, 1 in mode 9). Step rhythm and the MLC count take every 8th accelerometer sample (13 Hz). Log records stay valid, but steps/min reads about 4 % high [ASSUMED].
- Budget (`modes.c`): the module (4 exposures per sample), the LEDs (35 mA per sample) and the non-optical s4 terms for the interval and the stream. A 30 mAh cell lasts about 20 h (mode 6) or 11 to 12 h (modes 7 to 9): sessions, not daily wear.
- [VERIFY on the bench]: no FIFO overflow at 200 Hz (the FIFO is drained every 64 items, 80 ms); no dropped notifications at a 30 ms interval (watch the sequence numbers); SMP_AVE read back from 0x12 in modes 8 and 9, with words at 50 and 25 Hz; the measured current.

### 4.4 Vibration modes (research, bench capture)

Research only: a feasibility capture of whether the pod's accelerometer, pressed on the temporal region, picks up the wearer's own voice through the skull, and of jaw vibration. These modes are for the bench, not daily wear, and are never a default. There is no voice feature: nothing on the pod listens for speech or recognises words.

| | Mode 10 | Mode 11 |
|---|---|---|
| Accelerometer (LSM6DSOX) | 3333 Hz, high-performance, ±2 g | 6667 Hz, same |
| Output filter | LPF1 only (CTRL1_XL LPF2_XL_EN = 0): −3 dB at about ODR/2 = 1.67 kHz | same: about 3.33 kHz |
| Noise, 70 µg/√Hz × √bandwidth | 2.9 mg RMS | 4.0 mg RMS |
| Gyroscope, LEDs, MAXM86161A | off | off |
| Samples | 6667 B/s + 406 B/s of headers (29 notifications/s of 115) | 13333 B/s + 812 B/s (58/s), while capturing |
| I²C at 400 kHz | 0.56 s of every second | the whole bus: windows with gaps (below) |
| Connection interval | 15 ms | 15 ms |
| Current at the cell [ASSUMED], typ / worst | 1235 / 1518 µA | 1898 / 2587 µA |

**Vibration Data payload** (`firmware/src/vibcap.h`, `vib_encode`), little-endian:

| Offset | Type | Field |
|---|---|---|
| 0 | u16 | sequence number, +1 per notification |
| 2 | u16 | nominal rate, Hz: 3333 or 6667 |
| 4 | u32 | uptime ms of the first sample, the same clock as PPG Data and Motion Data [ASSUMED within one sample] |
| 8 | u32 | index of the first sample: the count of samples from the FIFO since the mode started, from 0 |
| 12 | u8 | mode, 10 or 11 |
| 13 | u8 | flags: bit 0 gap (the FIFO filled and restarted just before this packet's first sample), bit 1 a FIFO word that was not an accelerometer sample was skipped |
| 14 | i16 × n | samples, 0.061 mg per count at ±2 g (DS12814 Table 2). n is up to 115, (ATT MTU − 3 − 14) / 2 |

- **Axis.** The chip axis along the pod's z, out of the top face and so normal to the skin, with the pod's sign. In `atrium_pod.dts` (`imu-to-pod`) that is chip +Z. It follows that line if the layout changes [VERIFY: the gravity test of bring-up, and the mech drawing].
- **Select:** write `u8` 10 or 11 to Mode (0x0601) off the Plinth and subscribe to Vibration Data. No PPG Control bit is needed. On the Plinth the write fails with Value Not Allowed (0x13). Mode reads back the value while it runs. Packets are sized for the link's MTU at the Mode write. Request a 247-byte MTU before it.
- **Ends:** as §4.3: on disconnect, on the Plinth, at reboot, or when 0 to 3 is written. Night gives sleep, low battery gives lite, and off the body gives the probe. Going back powers the accelerometer down, empties its FIFO and sets low-power mode before the next mode's rate (`vib_transition`).
- **No optics, no wear check.** With the LEDs off the pod cannot test wear, so it keeps the wear state it had at the Mode write. Start the capture while worn. The green headroom limit does not apply. PPG Data, Motion Data, the pulse and HRV have no new data. Log records hold no pulse or motion for these minutes.
- **FIFO.** Accelerometer only, batched at the ODR (FIFO_CTRL3), in FIFO mode (stops when full), watermark 36 words. A thread reads FIFO_STATUS1/2 every 3 ms and reads 36 words (252 B, the nRF52832 TWIM's 255-byte limit) at a time, while the watermark is up. Each word is a tag and x, y, z. The pod keeps the one axis.
- **Gaps.** The sample index counts every sample that came out of the FIFO, so:
  - a notification lost on the link (no BLE buffer: at most 2 queued) shows as a sequence and index jump, and the index says exactly how many samples were lost;
  - when the FIFO fills, the pod drains it, restarts it and sets the gap flag on the next packet. The index runs on with no jump, and the uptime field gives the gap's length (± 1 ms).
  - The uptime field is correct across a FIFO stop. FIFO mode stops when full, so the words drained after the stop were made before it; their packets carry the time their first sample was made, worked out from the last running FIFO read and the sample period (`vib_cap_clock`), not the time they were read. So the last packets before a gap match the index time within 1 ms, and the gap's length is the uptime jump less the expected step.
- **Why mode 11 has gaps.** Each sample costs 7 bytes on I²C (the FIFO word holds all three axes). At 400 kHz, 9 bits per byte, the bus carries about 6000 words/s with the burst overhead. Mode 10 needs 3333/s, and mode 11 needs 6667/s, more than the bus has. In mode 11 the FIFO fills about 600 to 1700 words/s faster than it empties. It runs for roughly 0.3 to 0.8 s, stops when full, and the pod drains its 3 kB (about 90 ms) and restarts it [ASSUMED, bench]. Within a window the samples are contiguous. The nRF52832's TWIM tops out at 400 kHz, so no setting removes this. IMA-ADPCM would not help, because the limit is the I²C bus, not the radio. Gap-free 6.66 kHz needs SPI or the later IMU.
- **Link.** The pod asks for DLE (251-byte link packets) and 2M PHY when the mode starts. The build does neither on connect. A central that cannot do them keeps what it has.
  - At most 2 notifications are queued (`VIB_INFLIGHT_MAX`), leaving one TX buffer for the rest. That allows 2 × 244 B per connection event: 32.5 kB/s at 15 ms, or 16.3 kB/s if the phone holds 30 ms.
  - Mode 11 needs 14.1 kB/s, and mode 10 needs 7.1 kB/s. Both fit with DLE, on 1M or 2M PHY.
  - Without DLE the 244-byte notification splits into ten 27-byte link packets. Then mode 11 does not fit, and mode 10 may not.
  - iOS (DLE from iPhone 7 and iOS 10, 2M from iPhone 8) and Android 8 or later with a BT 5 radio should qualify [VERIFY with the target phones: watch the index for jumps].
  - If notifications are lost at a 30 ms interval, the fix is more buffers. Raise `CONFIG_BT_BUF_ACL_TX_COUNT` and `CONFIG_BT_ATT_TX_COUNT` to 6 and `VIB_INFLIGHT_MAX` to 4, at about 1.6 kB of RAM.
- **Anti-aliasing.** With LPF2 off the output is LPF1, whose cutoff the data sheet puts at ODR/2 in high-performance mode (DS12814 6.4.1). That is the Nyquist frequency itself, so the digital filter does not stop aliasing. The analog anti-aliasing filter in front of the ADC has no stated corner. Content near 1.67 kHz (mode 10) or 3.33 kHz (mode 11) may fold back [VERIFY on the bench with a swept shaker or tone]. If it does, LPF2 at ODR/4 (LPF2_XL_EN 1, HPCF_XL 000) gives about 833 Hz or 1.67 kHz, with real attenuation at Nyquist.
- **Current [ASSUMED]** (`modes.c`, s4 terms):
  - accelerometer high-performance: 170 µA on the 1.8 V rail, the same at any ODR (DS12814 Table 3), so 89 µA at the cell;
  - I²C, with HFCLK, EasyDMA, TWIM and the 4.7 kΩ pull-ups: 565 µA in mode 10, 933 / 989 µA in mode 11;
  - BLE at a 15 ms interval: 452 / 678 µA in mode 10, 651 / 1187 µA in mode 11;
  - CPU: 117 / 175 µA in mode 10, 214 / 310 µA in mode 11;
  - typ means 2M PHY; worst means 1M PHY, more CPU and, in mode 11, no gaps;
  - a 30 mAh cell lasts about 24 h (mode 10) or 16 h (mode 11).
- **Bench checks [VERIFY]:**
  - the burst read rolls the FIFO address back from 0x7E to 0x78, so one transfer carries many words (the data sheet does not say; bit 1 of the flags sets on a bad tag);
  - FIFO mode stays stopped once full until the restart (inside a window, index against time is a straight line);
  - the measured window and gap lengths in mode 11;
  - the axis and its sign;
  - the analog anti-aliasing corner;
  - no dropped notifications on the target phones;
  - the measured current.

### 4.5 Vibration bench rig (SPI, nRF52 DK; not the pod)

Before the EVT pod exists, the voice study (rig A) records mode 11 from an LSM6DSOX on an nRF52 DK. SPI removes the I²C limit that gives the pod's mode 11 its gaps (§4.4). The app is `firmware/tests/vib_bench` and uses `vibcap.c` unchanged.

- **Board.** nRF52 DK (PCA10040, nRF52832), on the bench. The DK's chip and toolchain match the pod's, so the packet code is the pod's own. LSM6DSOX on the ST STEVAL-MKI197V1 DIL24 adapter (LSM6DSOX and two 100 nF supply capacitors; [ST product page](https://www.st.com/en/evaluation-tools/steval-mki197v1.html)), hand-fitted. Any adapter with the bare chip works with the same signals.
- **Wiring.** Seven wires, under 10 cm. Pins from the adapter schematic (DB3721 Rev 1, Figure 1) and the DK's Arduino header (Zephyr `nrf52dk_nrf52832.dts`):

  | Signal | DK pin | nRF52832 | Adapter DIL24 pin |
  |---|---|---|---|
  | VDD and VDDIO | VDD (P1) | 3.0 V supply | 1 (VDD) and 2 (VDDIO), joined |
  | GND | GND | | 13 |
  | SPC (clock) | D13 | P0.25 | 20 (SCL/SPC) |
  | SDI (data in) | D11 | P0.23 | 21 (SDA/SDI) |
  | SDO (data out) | D12 | P0.24 | 22 (SDO) |
  | CS | D10 | P0.22 | 19 (CS) |
  | INT1 | not used | | 14, leave open |

  - **3.0 V, one rail.** The DK's VDD is 3.0 V [VERIFY with a meter]; the LSM6DSOX takes 1.71 to 3.6 V on VDD and 1.62 to 3.6 V on VDDIO (DS12814 Table 3), so it runs from the DK directly with no level shifting. The pod runs it at 1.8 V; the accelerometer's noise and sensitivity are specified the same across the supply range [ASSUMED].
  - CS low selects SPI. The firmware also sets I2C_disable (CTRL4_C bit 2), so SCL/SDA activity cannot be taken as I²C.
- **SPI.** SPIM2 with EasyDMA at 8 MHz, the nRF52832 SPIM's highest rate (the chip allows 10 MHz [VERIFY: DS12814 SPI timing table]). Mode 3 (clock idles high, data captured on the rising edge). A burst is the address byte and 36 FIFO words, 253 B, under the SPIM's 255-byte MAXCNT. Every transfer is 2 bytes or more, so nRF52832 anomaly 58 (an extra byte on 1-byte reads) cannot occur; Zephyr's workaround for it stays on as well.
- **Accelerometer.** Mode 11 exactly: `vib_transition(…, 11)` writes 6667 Hz, high-performance, ±2 g, LPF2 off, accelerometer-only FIFO in FIFO mode, watermark 36. Chip +Z, not negated, the pod's rule (§4.4 Axis). Mount the adapter with the chip's top face away from the skin, as on the pod. The gyroscope stays off.
- **Output.** USB virtual COM port of the DK's interface MCU, **1,000,000 baud**, 8N1, no flow control. 1 Mbaud is that port's top rate, and the nRF52832 UARTE makes it exactly (921,600 comes out at 941,176 baud). Record with any serial logger at 1,000,000 baud.
  - First a banner line starting `vib_bench:`. Then only packet lines: `0207 `, the packet as lowercase hex (the §4.4 payload: mode byte 11, rate 6667, index from 0 at start, uptime ms of the first sample, gap flag), `\n`. 115 samples a packet, 244 B, 494 characters a line; 58 lines/s, 28.6 kB/s, 29 % of the port.
  - Button 1 stops the capture, powers the accelerometer down and prints one `stats …` line: samples, bursts, the deepest FIFO level seen, gaps, SPI errors, dropped lines and the fewest free line buffers. A bench recorder should ignore both lines.
  - A line that finds no free buffer (16 lines, about 280 ms) is dropped and shows as an index jump, as a lost notification does on the pod.
  - LED 1 lights while streaming. LED 2 lights if the FIFO ever filled (a gap). LED 2 blinking means a set-up failure: the banner line says which.
- **Gap-free margin (simulated, `tests/vib_bench/host`).** A burst of 36 words costs about 0.3 ms of SPI (253 B at 1 µs each, plus up to 40 µs of driver and chip-select time [ASSUMED]) and carries 5.4 ms of samples, so the drain can move about 16 times the 6667 words/s it needs. With the 1 ms status polls the bus is busy about 9 % of the time. The FIFO holds 438 words, 65.7 ms at 6667 Hz [VERIFY: DS12814, FIFO size taken as 3 kB]. In the 60 s simulation, with a 3 ms hold-off every 17 ms, the FIFO never held more than 62 words (9.3 ms), and a 50 ms hold-off once a second still lost nothing. **Measured margin: none yet.** It is the stats line's `max_fifo_words` on the bench: 438 minus it, at 0.15 ms a word.
- **If the FIFO does fill** (the gap flag, LED 2), the words drained after it stopped carry their true times, as on the pod (§4.4 Gaps, `vib_cap_clock`), so the uptime field still gives the gap's length to about 1 ms.
- **Build and flash** (from the repo root): `firmware/tests/fw.sh bench` writes `firmware/build/vib_bench/zephyr/zephyr.hex`. Flash it with `nrfjprog --program firmware/build/vib_bench/zephyr/zephyr.hex --chiperase --verify --reset`, or copy it to the DK's JLINK drive.
- **Before recording:** turn the interface MCU's mass-storage drive off once (J-Link Commander: `MSDDisable`, then power-cycle the DK). With it on, the virtual COM port can lose or corrupt data at high rates on some hosts (SEGGER J-Link OB notes).
- **Bench checks [VERIFY]:** WHO_AM_I reads 0x6C (the banner says so); `gaps 0` and `lines_dropped 0` in the stats line after a 10-minute run; the FIFO burst rolls from 0x7E back to 0x78 (flag bit 1 never set); index against host time is a straight line at 6667 Hz; the axis sign (gravity +1 g when the chip's top faces up).

## 5. Skin temperature (0x0300): #3

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Skin Temperature | 0x0301 | R, N | `i16` skin (TH1, skin side under the window), `i16` top (TH4, under the lid), `u8` flags (bit 0 skin valid, bit 1 top valid), `u32` time. Notified at the mode's NTC rate: 10 s in sleep and off-body, every second otherwise, including on the Plinth |

Both sensors sit at the pod, so these are skin-side and pod temperatures, not core body temperature.

## 6. Activity (0x0400): #6 (V1); also an input to #4 and #5

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Activity | 0x0401 | R, N | `u32` time, `u16` activity index (mean of the absolute value of \|a\| − 1 g over the last 60 s, in mg), `u8` flags (bit 0 moving, bit 1 still for ≥ 10 min), then from the last log record: `u8` max motion level (0–7), `u8` class (0 still, 1 walk, 2 run, 3 other), `u8` steps/min (0 = none). Notified once a minute |

## 7. Log download (0x0500): #20

The pod writes one 12-byte record per 30 s epoch, day and night, into the 32 KB `log` partition of internal flash: 8 pages of 4 KB, 19.8 to 22.6 h without gaps (a night and a day between syncs). An epoch spent wholly on the Plinth is not logged. When the log is full, the oldest page is erased. The layout is fixed by the firmware encoder and the app decoder, which are tested against the same vectors.

**Blocks.** Each page holds blocks: an 8-byte header (`u32` time of the first epoch, `u16` block sequence, `u8` format 1, `u8` record count), then up to 255 records back to back. Epoch k of a block starts at time + 30 k. A block never crosses a page; when fewer than 20 bytes are left, the page tail stays erased (0xFF). A new block opens at any gap (Plinth, sampling stopped, reboot, clock set), at 255 records and at a page boundary. The block being written has its second header word erased (0xFFFFFFFF): count its records by byte 11, whose bits 4–7 are 0 in a written record.

**Record (12 bytes):**

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | pulse, bpm, mean of the epoch's confident seconds; 0 = none |
| 1 | u8 | bits 0–3 pulse confidence × 15, bit 4 on skin, bit 5 pulse from IR (green had no headroom), bit 6 docked, bit 7 charging |
| 2 | u8 | RMSSD over the epoch, ms (254 = 254 or more); 0 = fewer than 5 clean successive differences |
| 3 | u8 | bits 0–5 clean successive differences used (n_i, 0–63), bits 6–7 activity class: 0 still, 1 walk, 2 run, 3 other (`firmware/src/mlc.c`) |
| 4 | u8 | activity index, mg (mean of \|\|a\| − 1 g\| over the epoch), saturating at 255 |
| 5 | u8 | bits 0–2 highest MLC motion level, bits 3–7 pulse-present seconds (0–30) |
| 6 | u8 | skin TH1: 25.0 + 0.1 × value °C (1–254 = 25.1–50.4 °C); 0 = invalid or below range |
| 7 | u8 | steps in the epoch, saturating at 255 (step rhythm × 30 s in walk and run epochs [ASSUMED]) |
| 8 | i8 | head pitch, degrees, −90 to 90 (−128 = unknown: IMU off) |
| 9 | i8 | head roll, 2° per step, −90 to 90 = −180° to 180° (−128 = unknown) |
| 10 | u8 | oxygen-trend burst R × 100 for a burst that ended in this epoch (1–255 = 0.01–2.55); 0 = none |
| 11 | u8 | bits 0–3 that burst's confidence × 15; bits 4–7 always 0 |

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Log Info | 0x0501 | R | `u32` oldest page, `u32` head page (the one being written), `u16` page size (4096), `u16` epoch in s (30), `u32` pages in the log (8), `u8` format (1), `u8` record size (12). 18 bytes. Page numbers only grow; the pod holds oldest to head |
| Log Control | 0x0502 | W, I | Request: `u8` op, then arguments. Response, by indication: `u8` op \| 0x80, `u8` status (0 ok, 1 bad request, 2 busy, 3 out of range), `u32` value |
| Log Data | 0x0503 | N | `u32` page, `u16` byte offset in the page, then that page's bytes (a multiple of 4, up to the MTU) |
| Live Epoch | 0x0504 | N | `u32` epoch start (the log's time: Unix, or uptime with bit 31 set), `u32` head page, then the 12 record bytes exactly as logged. 20 bytes, one per logged epoch (none on the Plinth) |

| Op | Arguments | Effect |
|---|---|---|
| 0x01 read | `u32` from page, `u32` count of pages (0 = to the head) | Streams whole pages, erased tails included, on Log Data. The response's `value` is the number of pages that will be sent. The head page is sent as it stood at the request: bytes written after it, and the open block's second header word, read 0xFF. A page that leaves the ring while sending ends the read early. A read while one runs gets status 2 |
| 0x02 abort | none | Stops a read; no end indication |

**End of a read.** After the last Log Data packet the pod indicates `u8` 0x90, `u8` reason (0 all pages sent, 1 the page being sent left the ring), `u32` whole pages sent. It arrives after the last Log Data packet (one ATT bearer, no EATT). There is none after an abort or a disconnect. A new read drops an end indication not yet sent.

**Link.** While a read runs the pod requests a 15–30 ms interval, no latency, 4 s timeout [VERIFY: accepted on iOS and Android], and the mode's own interval again after it. In sleep mode (4 s) a full 8-page read would otherwise take minutes.

**Resume.** The page number is the cursor. Pages before the head are final; the head page grows, so the next sync reads it again from its start. Decode with the log decoder and drop epochs already seen by their time.

**Live.** While connected, Live Epoch brings each record as it is logged, with the same time as the log; store it like a decoded epoch (epochs are unique by time, so the next sync skips it). An uptime-stamped one (bit 31: the clock not set yet) waits for the sync's rebase. Its head page tells the phone when the head moved: a sync from the cursor then fills anything missed, for example notifications lost while out of range.

**HRV (#2).**
- Beats use the max-slope fiducial. An interval is clean when it is within 15 % of the median of the last 5 (`firmware/src/hrv.c`).
- Use only epochs with a low activity index: still, at rest or asleep.
- Overall RMSSD across several epochs is
√(Σ (RMSSD_i² × n_i) / Σ n_i), where n_i is byte 3 bits 0–5. Beats come from the 25 Hz PPG with parabolic timing, so it is a wellness estimate [VERIFY against ECG].

**Summaries are app-side.** Resting pulse, RMSSD, sleep timing and stages, active minutes and mean skin temperature are all computed from these records. Each record has a time and a quality value: pulse confidence, and RMSSD with n_i. Battery and the top NTC are not logged; they are live in Charge State.

**Sleep position (#5).**
- Pitch and roll come from the mean accelerometer gravity vector over the epoch, in the pod frame: pitch = atan2(−x, √(y² + z²)), roll = atan2(y, z).
- Pod frame. The pod is worn straight upright on the temporal region, long axis vertical, window and antenna end up, with no tilt; without glasses in the hollow above the cheekbone a thumb-width behind the outer eye corner, with glasses at eyebrow level with the arm under the pod (FEATURES #24, #26). +x along the long axis to the antenna end, +z out of the top face away from the head, +y = z × x (backward on the right temporal region, forward on the left). Firmware turns the chip axes into it with the board's `imu-to-pod` rotation: chip +Y = pod +x, chip +X = pod −y for U11 as placed [VERIFY by the bring-up gravity test].
- Upright gives pitch near −90° with roll as noise; a placement error moves each angle by no more than the error, so the app's classes allow about ±15°; lying puts gravity in the y–z plane, so pitch is near 0 and roll gives the side. Right temporal region: back −90°, face down +90°, on the pod's side ±180°, other side 0°; the left temporal region mirrors y. The Motion Data stream (0x0204) is in the pod frame too.
- The app classifies back, left, right, front and upright. The wear side comes from the app (#26); the app can refine the placement from a known upright moment.

**Signal check when pairing.** Write 1 to PPG Control, then read Pulse Detail. In smart mode, green is slot 1 (tag 0x01) at 25 Hz.

**Fit guide (#24).** Write 4 to PPG Control (fit mode) and subscribe to Fit Quality. Off the Plinth, fit mode holds smart mode (green at 25 Hz, no duty cycle) even before wear detection says worn. It ends when bit 2 is cleared, the phone disconnects, or 180 s pass. Fit Quality, 4 bytes, every second:

| Offset | Type | Field |
|---|---|---|
| 0 | `u8` | q, 0–100; 255 = not measuring (optics off, probe mode, or under 4 s since the mode began) |
| 1 | `u8` | Flags: bit 0 moving, bit 1 saturated, bit 2 dark, bit 3 the pulse LED is IR, bit 4 pulse present, bit 5 fit good |
| 2 | `u8` | Pulsatile perfusion pi in 0.01 % steps (255 = 2.55 % or more) |
| 3 | `u8` | Regularity rho × 100 |

- The value comes from the fit model in `firmware/src/sigq.c`, checked against its reference vectors in `firmware/tests/app/vectors`: q within 2, flags equal.
- It runs on the active pulse LED over the last 4 s: rho is the best normalised autocorrelation of the 0.6–3.5 Hz band-passed signal at 40–180 bpm; pi is band-passed RMS × √rho ÷ DC; q = 100 × s_pi × s_rho, where s_pi rises on a log scale from 0.03 % to 0.3 % and s_rho from 0.4 to 0.85.
- Saturated (a sample above 95 % of full scale) and dark (DC under 1 % of full scale) give q = 0. The pod has no LED AGC, so every mode runs the LED at its maximum for dark.
- Moving is the last second's band-passed accelerometer RMS above 30 mg. q is still sent; the app shows "hold still".
- Pulse present is wear detection's test: the unweighted pulse tracker steady for 5 s.
- Fit good = q ≥ 60, pulse present, and not moving, saturated or dark. The app shows green only on bit 5, so the pod and app agree; the bar alone can read high on noise. After the 10 s press, the fit is confirmed when bit 5 was set for at least 8 of the 10 s.
- The constants are per pulse LED (green, IR), the same values for now, and [ASSUMED] until V1 sets them from temporal-region data.

## 8. Settings (0x0600): #15, #19, #23

Stored in internal flash (Zephyr settings on the 8 KB `storage` partition, with the bonds), so they survive a reboot, power-off and ship mode.

| Characteristic | UUID | Props | Value |
|---|---|---|---|
| Mode | 0x0601 | R, W | `u8`: 0 full (green, IR, red), 1 smart (default, green), 2 lite (green, optics 30 s in every 120 s: a pulse every 2 min, no HRV), 3 sleep (green). All at 25 Hz (ML data contract). Research only: 6 (100 Hz), 7 (200 Hz), 8 (200 Hz averaged to 50 Hz) and 9 (200 Hz averaged to 25 Hz), green, IR, red and direct ambient (§4.3); 10 (3.33 kHz) and 11 (6.66 kHz), accelerometer only (§4.4). Not stored, for one connection |
| Time | 0x0602 | R, W | `u32` Unix seconds |
| Sleep Window | 0x0603 | R, W | `u16` start, `u16` end (minutes after local midnight), `i16` UTC offset in minutes. Inside the window the pod uses sleep mode. Default 23:00 to 07:00 |
| Log Interval | 0x0604 | R | `u16` seconds: the fixed 30 s epoch |
| Command | 0x0605 | W | `u8` op, `u32` key `0x41545249`: 0x01 reboot, 0x02 ship now (on the Plinth: BQ21061 ship mode at the next lift, 10 nA; off it: power off until the next Plinth), 0x03 erase log, 0x04 forget bonds (and disconnects), 0x05 factory reset (log, settings and bonds, then reboot) |

Write errors (`firmware/src/ble/gatt_atrium.c`, `gatt_encode.c`):
- Any settings write at a non-zero offset: 0x07 (Invalid Offset).
- Mode: 4, 5 or above 11, or a research mode on the Plinth: 0x13 (Value Not Allowed).
- Time: a length other than 4: 0x0D (Invalid Attribute Length).
- Sleep Window: a length other than 6, a minute value of 1440 or more, or an offset beyond ±14 h: 0x13. It applies only once Time has been written; start equal to end turns it off.
- Command: a wrong length, key or op: 0x13; a ship-mode failure: 0x80.
- PPG Control (§4.2): a length other than 1 or a value above 7: 0x13.

## 9. Firmware update: #21

MCUmgr SMP over BLE. Service `8d53dc1d-1db7-4cd3-868b-8a527460aa84`, characteristic `da2e7828-fbce-4e01-ae9e-261174997c48`. It supports the image and OS groups: upload, test, confirm and reset. It needs an encrypted link.

- MCUboot (swap using move, 32 KB) swaps between two 220 KB slots in internal flash; an image may be up to 216.9 KB.
- A new image confirms itself only after 60 s of healthy running: BLE up, PPG and IMU ready, a VSYS reading off the Plinth (or on the Plinth). If it is still unhealthy at 300 s, it reboots and MCUboot restores the previous image. An image that was never confirmed also reverts on any reset.
- The images are signed with ECDSA P-256. The development key is MCUboot's sample key [VERIFY: replace it before any release build].
- Version: `firmware/src/VERSION` alone sets the MCUboot header version and the x.y.z of Firmware Revision (0x2A26). The app compares the two to confirm an update; `fw.sh build` fails if they differ.
- Off the Plinth, image upload chunks are refused (MCUmgr EBUSY) while VSYS is below 3.60 V (`CONFIG_ATRIUM_DFU_MIN_MV`).
- This is the only field update path. The SWD pads on the base are for the programming jig and recovery.

## 10. Privacy: #23

- No account and no cloud is needed: all of the above works over a bonded link.
- The pod uses resolvable private addresses (`CONFIG_BT_PRIVACY`, a new one every 15 min), so it can't be tracked by its address. Nothing readable without encryption identifies one pod: the advertisement says only "an Atrium pod".
- The link is the only thing that guards the data (no account, no cloud): see Conventions, Security.
- Flash readout: on a production unit, lock debug access so a found pod's log and bond keys can't be read over the SWD pads. Lock in both images (`CONFIG_NRF_APPROTECT_LOCK=y` for the app and for MCUboot), and write UICR.APPROTECT at the end of the factory flash. After that the only way back is `recover`, a full chip erase that also wipes the log and the bonds. EVT units stay open [VERIFY: on the nRF52832 revision fitted, a locked part stays locked across reset with these two steps; to be decided before any release].
- Firmware updates take only images signed with the Atrium key (MCUboot ECDSA P-256). Replace the development key first (section 9).
- The data stays in pod flash until the app reads it, and command 0x05 wipes it.
