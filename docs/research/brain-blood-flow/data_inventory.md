# Step 0: data inventory (2026-11-08)

## Atrium temporal-region recordings

**None.** No EVT pod has been built, so no head recording, reference recording or protocol session exists. Every claim about what the temporal-region signal contains is therefore **unknown**. Only the physics prior (Monte Carlo) and method checks on other data are possible today.

## Hardware facts checked for this study

| Item | Value | Source |
|---|---|---|
| Optical module | MAXM86161, one photodiode | data sheet; `firmware/drivers/maxm86161` |
| LED centroids | green 520–535 nm, red 660 nm, IR 880 nm | data sheet, LED characteristics |
| Photodiode | peak 860 nm, 420–1020 nm | data sheet |
| LED-to-photodiode spacing | not dimensioned; about 1.5–2.5 mm from the 2.9 × 4.3 mm package | [VERIFY] package drawing F142A4+1 |
| ADC | 19 bit; noise 7 counts (green, IR) and 8.9 counts (red), RMS, at the data sheet's settings | data sheet |
| Firmware ADC setup | tINT 117.3 µs; 16 µA full scale; LED settling 12 µs | `maxm86161.c` |
| Ambient handling | in-module ambient cancellation; **no direct ambient slot**, so ambient leakage cannot be measured from the stream | `maxm86161.c`, `modes.c` |
| Sample rate | **25 Hz** for every slot, in every mode. Full mode streams green, IR and red | `modes.c`, `ble_gatt.md` 4.2 |
| IMU | LSM6DSOX, 12.5 Hz in every streaming mode | `modes.c` |
| Raw streams | PPG Data 0x0202 (FIFO words with tags); Motion Data 0x0204 | `ble_gatt.md` 4.2 |
| Time stamps | both streams carry pod uptime ms of the first sample, on the same clock [ASSUMED within one sample] | `ble_gatt.md` |

## What the study needs but the current firmware cannot give

**Update, same day:** firmware `ble_gatt.md` rev 14 adds research modes 6 (100 Hz) and 7 (200 Hz): green, IR, red and a direct-ambient slot (tag 0x04), with the accelerometer at 104 Hz. It answers items 1 and 2 below. Rev 15 adds modes 8 and 9: the same slots sampled at 200 Hz and averaged on the module to 50 and 25 Hz. All four are planned and not bench-tested; their currents are [ASSUMED].

1. **Sample rate.** 25 Hz is enough for amplitude, DC and optical-density analysis.
   - The loss for timing comes from point sampling without anti-aliasing (SMP_AVE = 1), not from the rate itself. On the finger data below, 95th percentile of |error per beat| against 500 Hz:
     - pulse-arrival time: 20 ms at 25 Hz;
     - IR–green lag: 8.8 ms at 25 Hz.
   - At 200 Hz these are 5 ms and 1.9 ms.
   - Morphology needs 200 Hz.
   - Details in `research_mode.md`.

   **Ask:** a research mode with green, red, IR and a direct-ambient slot at 100–200 Hz, and the accelerometer at 104 Hz. That is 1.2–2.4 kB/s, inside BLE throughput with data-length extension. It is a firmware change, so it is not made here.
2. **Ambient.** Without a direct-ambient slot, ambient leakage is only checkable with an LED-off run. The protocol adds a covered and uncovered 30 s check.
3. **Low battery** drops the green slot and shifts the others (`headroom.c`). Sessions must start above the green limit. The capture decoder assumes the full-mode slot order.

## Data that exists, and what it can and cannot test

| Set | Content | Licence | Usable for |
|---|---|---|---|
| pulse-transit-time-ppg 1.1.0 | 22 adults × sit / walk / run, 66 records of 484–551 s. Finger green 537, red 660 and IR 880 nm on a MAX30101 at 500 Hz. ECG with 46,405 checked R peaks, IMU | ODbL 1.0 | The methods on real three-wavelength PPG: quality, green-as-regressor, lag, pulse arrival, the sample-rate question (H6). **Not the head; no CO₂ or STA challenge** |
| Wrist PPG during exercise 1.0.0 | 19 records, wrist green PPG at 256 Hz, chest ECG | ODC-By 1.0 | Single wavelength: not useful here |
| sleep-accel 1.0.0 | wrist accelerometer, pulse, sleep stages | ODC-By 1.0 | Not useful here |
| Head-gesture IMU set | 6,408 rows of head IMU (nod, shake, swing, walk, still), no PPG | BSD-3 | Head-motion background only; no jaw labels |

No set has a capnometer, STA compression, a cold pack, temporal-region PPG or transcranial Doppler. **So there is no reference signal for any validation claim.**

## Step 0 checks on the finger set

Per-record checks (`data/finger/`):
- **Gaps:** no RR interval above 2 s in any record, so no dropped stretches.
- **Clock:** one MCU sampled everything within 2 ms, so there is no clock drift to check.
- **Clipping:** at most 0.002 % of samples sit at the record maximum.

Medians, with the quartiles in brackets:

| | Sit | Walk | Run |
|---|---|---|---|
| Perfusion index, green / red / IR | 3.5 / 0.71 / 0.33 % | 3.2 / 0.79 / 0.35 % | 3.5 / 0.81 / 0.36 % |
| Usable 8 s windows, green / red / IR | 97 / 100 / 100 % | 2 / 5 / 5 % | 0 / 0 / 0 % |
| Windows with motion > 50 mg | 0 % | 83 % | 100 % |
| IR on green, pulsatile R² | 0.88 [0.81, 0.94] | 0.83 | 0.79 |
| IR on green, slow (0.01–0.1 Hz) R² | 0.47 [0.35, 0.70] | 0.31 | 0.45 |

Notes on these numbers:
- 9 of 22 sit records are below 90 % usable on green. For 5 of them, green's perfusion index is above the 5 % upper limit set for the temporal region (the finger is better perfused). The other 4 fail on pulse regularity, or on perfusion outside the range in some windows (motion and clipping are zero).
- Walking and running are arm motion at the finger and say nothing about head motion.

## Pod capture checks, ready for the first recording

The capture converter turns a capture of 0x0202 and 0x0204 notifications into a session. It reports:
- lost notifications, from sequence gaps;
- picket-fence and invalid samples;
- sample gaps;
- pod-to-host clock drift in ppm, from the lower envelope of receive time against uptime.

For drift against a reference device, add a sync tap: three taps on the pod at the start and the end, visible on the pod IMU and on the reference device's own motion or event channel. The drift is the slope between the two tap times.
