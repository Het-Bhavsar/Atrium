# EVT validation protocol: body metrics

How the EVT pods are checked against reference sensors before any number is shown in the app. This is engineering validation for a wellness device: no accuracy or medical claim follows from it. Feature numbers (#n) are from [`docs/FEATURES.md`](../../docs/FEATURES.md).

Pass marks are [ASSUMED] proposals. Fix them before the first session and do not move them after seeing data.

## 1. Before anyone wears a pod

- **Ethics and safety:** written consent, a health screen before exercise, and a second person present. Stop on discomfort, dizziness, or at the participant's request. Get an ethics review before testing beyond the team or publishing [VERIFY].
- **LED exposure:** checked against IEC 62471 before first wear, because the pod sits near the eye [VERIFY].
- **Adhesive:** the product patch. Stop on irritation.
- **Data:** participant codes only, no names in files. Skin tone and age band are stored; nothing else that identifies anyone.
- **Pod firmware:** an EVT build that streams raw green, red and IR at the pod rate (25 sps) **and** raw accelerometer at 25–30 Hz with shared timestamps. It logs the normal 20-byte records at the same time, so the on-pod output and the raw data are checked together.
- **Clock:** sync the pod and every reference to the phone clock. Clap or tap the pod at the start and end of each session, so the accelerometer spike aligns the files to within 50 ms.

## 2. Participants

- **At least 12 adults** for V1 to V3, rising to 30 before V1 release [ASSUMED].
- **Skin tone:** use the 10-point Monk Skin Tone scale, read under daylight-balanced light at the temple. Optionally also take colorimeter ITA° readings. Recruit at least a third at Monk 7–10 and a third at Monk 1–3.
- Spread age, sex and hair. Note glasses, hair over the temple, and sweat.
- Report every result **per skin-tone group**, never only pooled.

## 3. Reference equipment

| Measure | Reference |
|---|---|
| Heart rate, RR intervals | Chest-strap ECG that records beat-to-beat RR (1 ms resolution), or a Holter ECG |
| Steps | Treadmill with set speeds, plus video or hand-tally counting |
| Sleep stages | In-lab polysomnography at a partner lab, or a home EEG sleep recorder with published PSG agreement [VERIFY availability] |
| Sleep position | Infrared video, scored in 30 s epochs by two raters (back, left, right, front, upright) |
| Oxygen-saturation trend | Finger pulse oximeter logging at 1 Hz (#8 only, trend comparison) |
| Eating, talking, clench | Video and a timed cue sheet |
| Wear | Event log from the operator (time on, time off, where the pod was) |

## 4. Sessions

### V1: Heart rate (#1, EVT gate)

1. **Seated rest, 5 min.**
2. **Treadmill**, 3 min per stage: 3, 5, 6.5 km/h walk; 8 and 10 km/h run (skip running for anyone not cleared); 3 min walk cool-down; 3 min seated.
3. **Daily-life block, 20 min:** stairs, typing, talking, a snack, standing up and sitting down.

- **Analysis:**
  - Run the logged pod HR (1 Hz) and the raw stream through `atrium_ml.hr.spectral_hr` (and the firmware build), against the chest-strap HR over the same 8 s windows (`hr.reference_hr`).
  - Report MAE, share within 5 bpm, and coverage (share of seconds with confidence ≥ the threshold), per stage and per skin-tone group.
  - Also report the artefact ratio: RMS of the green signal after removing the ECG-locked average pulse, divided by the pulse RMS.
- **Pass [ASSUMED]:**
  - MAE ≤ 3.5 bpm at rest and walking.
  - MAE ≤ 5 bpm running.
  - Coverage ≥ 90 %.
  - No skin-tone group's MAE more than 1.5 bpm above the best group's.
  - If dark skin fails on green, test the IR fallback on the same data before changing hardware.

### V2: Steps and activity class (#6)

- Within the V1 treadmill stages, count steps by video.
- Then 10 min of free walking outdoors, including stairs, with a hand-tally.
- 2 min each of confounders: chewing, nodding, talking while seated.
- **Analysis:** pedometer count error per stage; false steps per minute in the confounders; confusion matrix of the activity class.
- **Pass [ASSUMED]:** step count within 5 % on the treadmill and 10 % outdoors; under 5 false steps per minute in the confounders.

### V3: HRV (#2)

1. 5 min seated.
2. 10 min supine.
3. 5 min seated paced breathing at 6 breaths per minute, which raises RMSSD and tests range.
4. Overnight, the chest strap or Holter worn during V4 nights.

- **Analysis:**
  - Compute RMSSD per 60 s and per 5 min from pod beats (`atrium_ml.hrv`) and from ECG RR, both with the same 20 % ectopic filter on the ECG.
  - Report bias, MAE, limits of agreement and r.
  - Repeat for the nightly pooled value, built from still records only (activity index below the still threshold).
- **Pass [ASSUMED]:** 5 min RMSSD bias within ±5 ms; MAE ≤ 15 % of the reference median; nightly pooled r ≥ 0.9.

### V4: Sleep and sleep position (#4, #5)

- **Nights:** at least 2 per person, for 10 people, 20 nights in all, with the stage reference and infrared video.
- **Pod placement:** worn on the temple the person chose. Note the pillow type.
- **Analysis:**
  - Sleep/wake and wake / non-REM / REM per 30 s epoch from `atrium_ml.sleep`, trained only on other people's nights (leave one person out). Report accuracy, Cohen's κ, wake detected, and total sleep time error in minutes.
  - Position: `atrium_ml.position` against the video score. Report agreement, κ, and minutes per position per night. Report "upright" separately: sitting up in bed.
- **Pass [ASSUMED]:**

  | Measure | Pass |
  |---|---|
  | Sleep/wake κ | ≥ 0.45 |
  | Total sleep time error | Within ±30 min on 80 % of nights |
  | Position agreement | ≥ 90 % |
  | Side-lying vs back confusion | ≤ 5 % of epochs |

### V5: Oxygen-saturation trend (#8, Later)

- On V4 nights, the finger oximeter runs alongside.
- **Analysis:** correlate the pod's ratio trend (ΔR, from the night median) with the oximeter's 1 Hz series, lag-aligned. Count dips found by each.
- **Pass [ASSUMED]:** the trends move together (r ≥ 0.6 over dips) before the feature is even planned.
- No percentage is ever shown, and no claim follows.

### V6: Eating detection, with jaw-clench and talking confounders (#11, Later)

A seated, 25 min, cue-timed session with video:

| Block | Time |
|---|---|
| Rest | 2 min |
| Crackers | 3 min |
| Apple | 3 min |
| Gum | 3 min |
| Drinking sips | 2 min |
| Reading aloud | 3 min |
| Conversation | 3 min |
| Jaw clench, light / medium / hard, 10 × 3 s each, cued | 3 min |
| Nodding and head turns | 2 min |
| Walking while talking | 1 min |

Then a free-living day with a meal diary.

- **Analysis:** per 10 s epoch, chewing vs not, F1 per block. False eating prompts per hour on the free-living day.
- **Pass [ASSUMED]:**
  - F1 ≥ 0.85 for chewing.
  - Talking and clench blocks ≤ 10 % false chewing epochs.
  - ≤ 1 false prompt per day.

### V7: Wear detection (#18, EVT gate)

Scripted on the bench and on people; the operator logs ground truth:

1. Worn seated for 10 min, then lifted into the air.
2. Placed skin-side up and skin-side down on a table, under a lamp and in the dark.
3. In a trouser pocket while walking for 10 min.
4. In a bag.
5. Held in the hand.
6. Docked.
7. Worn with hair over the window.
8. Worn during the V1 treadmill run.

- **Analysis:** off-body detection latency per scenario; false off-body seconds while worn; false on-skin seconds while off.
- **Pass [ASSUMED]:**

  | Measure | Pass |
  |---|---|
  | Lifted | Detected ≤ 5 s |
  | Table | Detected ≤ 90 s |
  | Pocket | Detected ≤ 6 min |
  | False off-body while worn | 0 events during exercise; ≤ 1 per day |
  | False on-skin while off | ≤ 1 % of off-body time |

  Set the [ASSUMED] thresholds in `atrium_ml/wear.py` from this data only.

## 5. Reporting

One results file per session set, made by the same `atrium_ml` functions. It includes every participant, including failures and dropped data with the reason. Numbers reach `docs/` only when they pass, with the cohort size and skin-tone spread next to them.
