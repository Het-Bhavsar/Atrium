# Atrium body-metric prototypes

Reference implementations in Python of the signal processing behind [`docs/FEATURES.md`](../../docs/FEATURES.md). They are written for the Rev B.2 pod: MAXM86161 green/red/IR PPG (one photodiode), LSM6DSOX accelerometer, and skin-side NTCs for temperature.

Each module's docstring is its specification. The on-pod parts are written so they port to plain C on the nRF52832.

Wellness and research use only. These are estimates, not medical measurements. Nothing here has been validated on an Atrium pod yet: that is [`VALIDATION.md`](VALIDATION.md).

| Module | Feature | Runs on | Method (Atrium's own) |
|---|---|---|---|
| `atrium_ml/hr.py` | #1 heart rate | pod | "Spectral track": 8 s FFT of green PPG, accelerometer spectrum subtracted when moving, peak tracking |
| `atrium_ml/hrv.py` | #2 HRV | pod (beats), app (nightly) | "Max-slope PPI": upstroke-slope beats with sub-sample timing, median-based interval cleaning, RMSSD |
| `atrium_ml/wear.py` | #18 wear | pod | DC per mA, steady pulse peak, skin NTC level and slope, in a state machine |
| `atrium_ml/position.py` | #5 sleep position | app (from the logged pitch and roll of the mean accelerometer vector) | "Skin-normal head frame": the pod normal is the head's lateral axis; vertical learned from walking |
| `atrium_ml/sleep.py` | #4 sleep | app | 6 features from the existing log fields, logistic regression |
| `atrium_ml/synth.py` | — | tests | Synthetic PPG, beats and head motion with known truth |
| `benchmark.py` | — | desktop | Runs all of the above on public PhysioNet records |
| `make_test_vectors.py` | — | desktop | Synthetic input and expected-output CSVs for porting hr, hrv and wear pulse presence to firmware |

## Run

From the repo root:

```
pip install -r app/python/requirements.txt
python3 -m pytest app/python
python3 app/python/benchmark.py --ptt DATA/ptt --wrist DATA/wrist --sleep DATA/sleep-accel \
    --out app/python/results/benchmark.json
python3 app/python/make_test_vectors.py OUT_DIR    # firmware port references, no dataset needed
```

Getting the data (about 3.5 GB in all; keep it outside the repo). Use each PhysioNet page's ZIP, or:

- `DATA/ptt`: `python3 -c "import wfdb; wfdb.dl_database('pulse-transit-time-ppg', 'DATA/ptt')"`
- `DATA/wrist`: `python3 -c "import wfdb; wfdb.dl_database('wrist', 'DATA/wrist')"`
- `DATA/sleep-accel`: the `labels/`, `motion/` and `heart_rate/` folders (not WFDB, so ZIP only).

Check that every `.hea` has its `.dat` and `.atr`: a broken download skips a record silently.

## Results on public data (`results/benchmark.json`)

Every PPG was resampled to **25 Hz**, the pod rate, before processing. **None of these datasets is head-worn:**
- The PTT records are a finger clip with the subject on the spot (sit, stationary walk, run).
- The wrist and sleep records are from the wrist.

So these numbers show the methods work; they are not the pod's accuracy.

### #1 Heart rate vs ECG (8 s windows, 1 Hz)

**Pulse Transit Time PPG** (22 adults, 66 records, about 8 min each). Each cell is MAE in bpm, with the share of windows within 5 bpm in brackets:

| Method | Sit | Walk | Run |
|---|---|---|---|
| Spectral track with the accelerometer | **1.5** (97 %) | **3.2** (87 %) | **2.2** (94 %) |
| Spectral track, no accelerometer | 1.5 (97 %) | 8.7 (78 %) | 3.5 (93 %) |
| Current firmware autocorrelation (`firmware/src/pulse.c`, ported) | 1.2 (95 %) | 10.1 (74 %) | 8.1 (77 %) |

**Wrist PPG During Exercise** (8 adults, 19 records; not used for tuning). MAE in bpm:

| Method | Bike, low resistance | Bike, high resistance | Walk | Run |
|---|---|---|---|---|
| Spectral track with the accelerometer | 13.0 | 7.8 | 65 | 55 |
| Spectral track, no accelerometer | 29 | 45 | 41 | 49 |
| Firmware autocorrelation | 37 | 52 | 28 | 53 |

**The wrist walk and run results fail:** the track locks onto arm-swing harmonics near the step rate, starting from motion with no rest period to lock on. The temple has no arm swing, but V1 in `VALIDATION.md` must measure the temple's own artefact before HR under motion is trusted. Two ways to fix it:
- acquire HR only at rest and track from there;
- add an adaptive (NLMS) pre-filter on the accelerometer.

The tuned parameters (`mask_power`, `moving_f_lo`, re-acquisition) were chosen on the PTT walk and run records, so the PTT motion numbers are optimistic.

**Confidence:** the HR confidence rates the track. After motion subtraction, noise scores 0.2–0.7, so it does not say a pulse is present. `wear.pulse_present` does that.

### #2 RMSSD vs ECG (PTT sit records, 22 people, 60 s windows)

The reference is the ECG R-peaks with a 20 % ectopic filter.

| PPG rate, timing | Windows | Bias (ms) | MAE (ms) | 95 % limits (ms) | r |
|---|---|---|---|---|---|
| **25 Hz, parabolic** | 163 | +1.8 | **3.0** | −6.4 to +9.9 | 0.94 |
| 25 Hz, cubic | 163 | +2.3 | 3.2 | −5.6 to +10.1 | 0.94 |
| 25 Hz, no interpolation | 163 | +11.5 | 11.7 | −1.3 to +24.3 | 0.85 |
| 50 Hz, parabolic | 163 | +2.6 | 3.7 | −8.2 to +13.3 | 0.90 |
| 100 Hz, parabolic | 163 | +3.4 | 4.5 | −8.8 to +15.6 | 0.87 |

- The median reference RMSSD was 30 ms.
- Per person (all windows pooled, the `docs/ble_gatt.md` rule), MAE was 2.5 ms and r 0.96.
- 25 Hz with parabolic timing was the best tested rate: higher rates let more slope noise through the first difference.
- The interval rule (within 15 % of the median of the last 5 intervals) beat 10 % and 20 %.

### #18 Wear detection

The on-skin data is all 66 PTT records: 8.9 h of sit, walk and run, with each record's own sensor temperature. The off-body cases are synthesised from each record's own AFE noise, a 4 min cooling constant [ASSUMED], and motion.

| Case | Result |
|---|---|
| Worn | 0.01 % of seconds wrongly called off-body |
| Lifted into open air | Off after 3 s |
| On a table under a lamp (lit, still, cooling) | Off after 59 s |
| In a pocket while walking | Off after a median of 299 s. 2 of 66 runs were not caught within 10 min. 2.7 % of later seconds were wrongly back on |

Noise passes for a pulse too easily, so pulse presence needs a steady peak for 5 s on the unweighted spectrum, away from the accelerometer peak and its half and double. Every threshold is [ASSUMED] until bench test V7.

### #5 Sleep position

No public head-worn IMU dataset has position labels, so this is a Monte Carlo test. Each run used 2000 random pod placements (any twist, tilt up to the limit), the head turned up to 25° on the pillow, and upright learned from simulated walking.

| Placement tilt up to | 10° | 20° | 30° | 40° |
|---|---|---|---|---|
| Positions correct | 100 % | 100 % | 100 % | 99.7 % |

This only shows the geometry holds; it does not test people. The real errors will be half-side lying and pillow roll, which V4 measures against video.

### #4 Sleep (sleep-accel: 31 lab nights, wrist motion and pulse, sleep stages scored every 30 s)

Each night is scored by a model trained on the other 30.

| Model | Accuracy | κ | Wake epochs found | Sleep-time error per night |
|---|---|---|---|---|
| Sleep / wake, motion only | 0.90 | 0.37 | 43 % | −0.2 min mean, 21 min MAE |
| Sleep / wake, motion + pulse | 0.90 | 0.38 | 44 % | −1.3 min mean, 23 min MAE |
| Wake / non-REM / REM | 0.58 | 0.28 | wake 62 %, non-REM 59 %, REM 53 % | — |

Pulse adds little here: the dataset's pulse is sparse. Stage estimates are weak, so the app must label them as estimates (FEATURES.md #4).

## Datasets and licences

| Dataset | Licence | Use here |
|---|---|---|
| [Pulse Transit Time PPG 1.1.0](https://physionet.org/content/pulse-transit-time-ppg/1.1.0/) | ODbL 1.0 | HR, HRV, wear. Any database derived from it that we publish must be ODbL. None is in this repo |
| [Wrist PPG During Exercise 1.0.0](https://physionet.org/content/wrist/1.0.0/) | ODC-By 1.0 | HR, held out |
| [sleep-accel 1.0.0](https://physionet.org/content/sleep-accel/1.0.0/) | ODC-By 1.0 | Sleep |

No dataset files are in the repo. The code is Apache-2.0 (`app/LICENSE`).
