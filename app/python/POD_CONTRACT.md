# Pod data contract: MLC motion level and oxygen trend

What firmware (`firmware/`) implements for `atrium_ml/mlc.py` (#6) and `atrium_ml/spo2.py` (#8, Later).

- **Reference vectors:** `python3 app/python/make_test_vectors.py OUT_DIR`, the `mlc_*` and `spo2_*` files.
- **Pod:** Rev B.2, with the LSM6DSOX (U3, I²C 0x6A) and the MAXM86161 (green LED1, IR LED2, red LED3, one PD).
- **Currents:** at the cell, using firmware's MAXM86161 model: module 1.33 µA per exposure per sps; LED I × 123.8 µs × 1.21 per pulse.

## 1. MLC motion level (#6)

| Item | Value |
|---|---|
| Accelerometer | 12.5 Hz, ±2 g, low-power mode: the worn-mode setting. Off on the dock (the MLC stops with it) |
| Register program | `mlc.atrium_config()`, 97 writes in order, after the driver's init. The vectors' `mlc_config.csv` holds the same list. It ends with CTRL1_XL = 0x10 (12.5 Hz, ±2 g) and CTRL2_G = 0x00 (gyroscope off) |
| Do not | Change the accelerometer ODR below 12.5 Hz or the full scale while the MLC runs. Raising the ODR is fine: the MLC keeps its own 12.5 Hz |
| Reading the level | INT2 or polling both work. Firmware polls MLC0_SRC every 39 samples; its phase can drift against the MLC's own window, so a poll can repeat or skip a level. That is harmless for the record's max and median |
| Output | MLC0_SRC (0x70): level 0–7, one every 39 samples (3.12 s). A pulse on **INT2** (P0.11) at each update |
| Interrupt routing | MD2_CFG (0x5F) = 0x02 on the main page, MLC_INT2 (0x11) = 0x01 on the embedded page [VERIFY both addresses in the LSM6DSOX datasheet on the bench] |
| Bench check | Pod still on a table: level 0. Shaken by hand: 5–7. Walking with the pod on the head: 3–5 |
| Current | MLC on top of the running accelerometer: [VERIFY datasheet; ASSUMED 1–3 µA]. One I²C read per 3.12 s: about 0.2 µA |

**Activity class (MCU, once per log record):**
- Inputs: the record's MLC levels, plus `mlc.step_rhythm` on the last 100 accelerometer samples at 12.5 Hz.
  - Take |a|, subtract the mean, apply a Hann window and a 128-point real FFT.
  - Peak in 1–3.5 Hz, parabolic-refined; share = power within 0.15 Hz of the peak over the 1–3.5 Hz power.
- Rules, from `mlc.activity_class`:

| Class | Condition |
|---|---|
| still | every level is 0 |
| walk | median level 4–5, share ≥ 0.65 and 70–150 steps/min |
| run | median level ≥ 6, share ≥ 0.65 and 140–200 steps/min |
| other | anything else |

- Every threshold is [ASSUMED] until bench test V2.
- **Cost:** the HR code's 8 s accelerometer ring can feed it, decimated to 12.5 Hz. The 128-point FFT scratch (0.5 KB) can share the HR FFT buffer, since they run one after the other. About 50 B of state.

**Not delivered:**
- **A chewing tree (#11):** no temple IMU data that Atrium may use exists. It needs 52–104 Hz and a V6 recording session first. Then it becomes a second tree in the same program.
- **An MLC wear hint:** the IMU cannot tell a still head from a pod on a table. Wear stays with PPG and NTC (`wear.py`).

## 2. Oxygen trend bursts (#8, Later)

| Item | Value |
|---|---|
| Burst mode | G10 / IR10 / R10 at 25 Hz (sleep slots plus red and IR); in smart, G15 / IR10 / R10 at 25 Hz. One rate for all slots |
| Burst length | 30 s. The first 4 s settle; then 4 s windows stepped by 2 s, 12 windows |
| AGC | Frozen for the whole burst. The ratio needs constant LED current; a mid-burst change breaks DC |
| When, sleep | Every 5 min, if the MLC level was 0 for the previous 60 s and the pulse is present. Otherwise retry each minute |
| When, smart | Every 30 min under the same stillness test. None in full, lite or off-body |
| Current | Burst optics 212 µA vs 71 µA sleep (+141 µA while on). Sleep, 10 % duty: **+14 µA**. Smart, about 1.7 % duty: **+2.4 µA**. Daily average about **+6 µA** |
| Pipeline | `spo2.windows` and `spo2.burst`. Per window: DC = mean; AC = RMS of the 0.5–4 Hz band-pass (2 biquads per channel, primed at the burst start); R = (AC/DC)red / (AC/DC)IR |
| Gates (all [ASSUMED]) | Red/IR band-passed correlation ≥ 0.8; IR perfusion ≥ 0.02 %; motion ≤ 15 mg; green pulse present; both channels within 5–95 % of the 19-bit range |
| Burst output | R = median of good windows. Confidence = good share × (1 − IQR/median / 0.2), clipped to 0–1. 0 when fewer than 4 windows pass |
| Calibration | **On the phone, not the pod.** The pod sends R and confidence only. The app turns the night into a percentage with `spo2.Calibration`, but only after V5 has fitted and validated it on Atrium pods. Until then it shows the trend |
| RAM | 6 running sums per window plus 12 window results: under 0.3 KB. Flash about 2 KB [ASSUMED] |

**App:**
- `spo2.night_ratio` gives the confidence-weighted median of the night's bursts.
- `spo2.nightly_percent` gives one whole-percent nightly value, only with a validated calibration and at least an hour of good bursts.
  - Capped at 100.
  - Below the calibrated range it shows "lower than usual".
  - No per-minute values and no alerts.
- `spo2.trend` gives the relative change against the median of the last 14 nights. It shows nothing before 7 nights.

## 3. Budget

| Feature | RAM | Average current |
|---|---|---|
| MLC level + class | about 0.05 KB (+0.5 KB FFT scratch if not shared) | about 1–3 µA [VERIFY] |
| Oxygen trend | under 0.3 KB | +14 µA in sleep, +2.4 µA in smart, about +6 µA daily |
| Total | under 1 KB of the 4 KB allowed | |

## 4. Log record and GATT (one batched layout change)

The record grows from 20 to **24 bytes**. It also carries the earlier ask for pulse-present seconds.

| Offset | Type | Field |
|---|---|---|
| 20 | u8 | Bits 0–2: max MLC level in the record. Bits 3–5: median level. Bits 6–7: class (0 still, 1 walk, 2 run, 3 other) |
| 21 | u8 | Step rhythm in steps/min from the record's last window; 0 = none. The app sums steps as rhythm × time in walk/run records |
| 22 | u8 | Bits 0–3: pulse-present seconds (0–15). Bits 4–7: oxygen-trend burst confidence × 15, 0 = no burst ended in this record |
| 23 | u8 | Burst R × 100 (1–255 = 0.01–2.55), 0 = none |

Firmware keeps the 15 s interval at 24 bytes. On the EVT Pill board's 16 Mbit flash that is about 11.8 days of log (firmware, 2026-11-07), so the app should sync at least weekly. The app keeps the 14-night baselines for HRV, recovery and blood oxygen on the phone, so they do not depend on the pod log length.

### 4b. EVT baseline: compact 30 s record (owner decision, 2026-11-07)

The EVT baseline (`atrium-evt-baseline/`) has no log flash, only about 24 KB of storage. On that build the log is
**one 12-byte record per 30 s epoch, day and night**, in place of the 15 s record above. The full layout and a
reference encoder/decoder are in `atrium_ml/logrec.py`.

| Item | Value |
|---|---|
| Blocks | 8-byte header (u32 first-epoch time, u16 sequence, u8 format 1, u8 count), then up to 255 records. A new block at every gap (dock, sampling stopped, reboot), at 255 records, and at a page boundary |
| Pages | The log is a ring of whole 4 KB flash pages, and a block never crosses one. When fewer than 20 bytes are left, the page tail stays erased (0xFF). A full page holds 340 records with no waste. When the log is full, the oldest page is erased |
| Writing the header | The time is written when the block opens. The second header word (sequence, format, count) stays erased until the block closes. On boot, firmware closes any open block by counting its records. The decoder also reads an open block |
| Reserved bits | Byte 11 bits 4–7 are always 0 in a written record. Erased flash reads 0xF, which is how an open block's records are counted. Never use these bits for data |
| Time before the first sync | Seconds since boot with bit 31 set. On the first connect the app rebases those blocks from the pod's current uptime. It drops blocks from an earlier boot, whose uptime base is lost |
| Capacity | `logrec.capacity_hours`. With 4 log pages (16 KB, after settings and bonds): **8.5–11.3 h**, so one night only if the phone syncs before bed. With 8 pages (32 KB, if MCUboot shrinks from 48 to 32 KB): **19.8–22.6 h**. Every gap shortens it slightly. ML needs at least one full night plus margin, so **8 pages are recommended** [firmware and owner decision] |
| Per epoch | Pulse and confidence; on-skin, IR-source, docked and charging flags; RMSSD (1 ms) + clean differences; activity class; activity index (mg); highest MLC level; pulse-present seconds; skin TH1 (0.1 °C from 25 °C); steps; pitch (1°) and roll (2°); burst R × 100 + confidence |
| Dropped vs the 15 s record | VBAT/SoC (sent live over GATT), the edge/top NTC, and beat count. Thermal trends (#9, Later) would need the top NTC back in a later format |
| Effect on the phone models | Built on the 30 s epoch `sleep.py` already uses. Rounding activity and pulse to whole numbers moved sleep/wake κ from 0.379 to 0.375 on sleep-accel; sleep-time error unchanged at −1.3 min mean. 1 ms RMSSD steps move the nightly pooled RMSSD by under 0.1 ms (test) |
| HRV per epoch | Beats are counted into the epoch whose 30 s they end in. RMSSD needs at least 5 clean differences, as now |

Reference vectors for the codec are the encode/decode tests in `tests/test_logrec.py`. The byte layout is fixed by
`logrec.encode`.

**GATT:**
- Activity (0x0401): append u8 max level, u8 class and u8 steps/min.
- New Oxygen Ratio (0x0205): R, N. Fields: u32 time, u16 R × 1000, u8 confidence (0–255), u8 good windows. Notified at the end of each burst.
- `docs/ble_gatt.md` describes it as "relative red/IR ratio, research only".
