# Atrium features

The single list of what Atrium does, for hardware, firmware, app and cloud work. A feature is built only once it is listed here. Change it by pull request; every workstream reads it.

Atrium is a wellness and research device. Every feature below gives wellness trends, never a diagnosis, treatment or medical measurement (see `certification.md`).

**Status:**
- **EVT:** the first build must support it end to end.
- **V1:** first public release.
- **Later:** a research item, built after its signal is validated on the bench.

Sensors are the EVT pod's: PPG module MAXM86161A (green, red and IR, one photodiode), IMU (LSM6DSOX), two 10 k NTCs (TH1 skin, TH4 top), VSYS sense (no fuel gauge), BLE (nRF52832), and charge state from the BQ21061. The GATT interface is in `ble_gatt.md`.

## Body metrics

| # | Feature | What the user sees | Source | Status | Notes |
|---|---|---|---|---|---|
| 1 | Heart rate | Live and 24 h HR, resting HR, HR zones in workouts | PPG green, 1 Hz output | EVT | Validate against a chest strap during treadmill steps, light skin to dark skin, and publish the result |
| 2 | HRV | Nightly RMSSD and its trend | PPG beat-to-beat, at rest and asleep | V1 | Only in still, low-motion windows |
| 3 | Skin temperature | Nightly deviation from the user's own baseline | TH1 skin NTC | EVT | Relative trend, not body temperature |
| 4 | Sleep | Duration, timing, an estimate of stages, wake-ups | HR, HRV, IMU | V1 | Stages are an estimate; label them that way |
| 5 | Sleep head position | While asleep only: which way the head lies through the night, as time on the back, front, left and right sides, and **time lying on the pod's side** (head on the pillow on the side the pod is worn). If most of the night is on the pod's side, the app suggests the other side for the next pad | IMU orientation and the wear side (#26) | V1 | Head orientation from the accelerometer. Left and right are named from the wear side, so #26 must be right. Lying on the pod presses it into the pillow: its readings in those epochs are marked lower quality |
| 6 | Activity and effort | Active minutes, workouts detected, an effort score from HR zones | IMU, HR | V1 | Atrium's own effort score, with the formula documented in `app/` |
| 7 | Recovery | A morning score from resting HR, HRV, sleep and temperature deviation | 1–4 | V1 | Formula documented and versioned |
| 8 | SpO₂ trend | Overnight relative oxygen-saturation trend | PPG red and IR | EVT | The pod logs a red/IR ratio from 30 s bursts: every 5 min asleep, every 30 min in Smart. Uncalibrated, so trend only. Never a % SpO₂ number |
| 9 | Thermal trends | Skin-temperature and pod-gradient trends over the day and during exercise | TH1 and TH4 NTCs | Later | Atrium's own physiology; no composite "score" until validated |
| 10 | Perfusion trend | Relative PPG amplitude (AC/DC) trend | PPG | Later | No brain or cerebral claims (see [brain-blood-flow.md](brain-blood-flow.md)) |
| 11 | Eating detection | A prompt: "Looks like you are eating — log it?" | IMU jaw-motion pattern | Later | Opt-in. Jaw clench and talking are confounders, so test before shipping |
| 12 | Cycle-aware view | Temperature-based phase estimate and day labels | 3 plus user input | Later | Opt-in, wellness only, never contraception guidance |

## Coaching

| # | Feature | What the user sees | Status | Notes |
|---|---|---|---|---|
| 13 | Daily summary | Morning card: recovery, sleep and one suggestion | V1 | Template text first |
| 14 | Assistant | Ask questions about your own data, e.g. how to wind down tonight | Later | Cloud and opt-in. Answers cite the user's numbers and carry the wellness disclaimer |

## Device and system

| # | Feature | What the user sees | Source | Status |
|---|---|---|---|---|
| 15 | Pairing | BLE pairing and bonding, device name, firmware version | BLE | EVT |
| 16 | Battery and charging | Pod % in 10 % steps with the time left for the chosen mode (#27); charging, on the Plinth, full; cold or hot "charging paused" | VSYS sense, BQ21061 /PG and status, TS limits | EVT |
| 17 | Plinth status | "On the Plinth: charging / charged" | BQ21061 /PG and status | EVT |
| 18 | Wear detection | Off-body, so pause measurement and save power | PPG DC and pulse, TH1, IMU motion | EVT |
| 19 | Ship mode | Store and ship with the cell disconnected: set on the Plinth, takes effect on lift, wakes on the Plinth | BQ21061 ship mode | EVT |
| 20 | Log sync | Compact offline log on the pod (about 20 h: 19.8–22.7 h, 12 B per 30 s epoch) and sync when the phone is near; the app shows when it last synced and warns after 16 h without a sync that older readings will soon be overwritten. Background sync on Android and iOS is not done yet | Internal flash, GATT log service | EVT |
| 21 | Firmware update | Over the air, signed, with rollback | MCUmgr SMP | EVT |
| 22 | Data export and delete | CSV export of the user's data; delete everything | App, cloud | V1 |
| 23 | Privacy | Works without an account. Cloud features are opt-in. No sale or sharing of health data | App, cloud | EVT |
| 24 | Fit guide | First wear and every pad change: "Do you wear glasses?" picks the spot (no glasses: the temporal hollow, above the cheekbone and a thumb-width behind the outer corner of the eye; glasses: higher, at eyebrow level, with the arm passing just under the pod). Pod upright, window end up. A live signal bar for the last few millimetres, then "press for 10 s". The bar is Fit Quality (`ble_gatt.md`, 0x0206): it turns green only on the fit-good flag, and the press is confirmed when the fit was good for 8 of the 10 s. The glasses answer is kept and can be changed in Settings | PPG signal quality, IMU orientation | EVT |
| 25 | Skin pad | Step-by-step pad change in the app; reminders to change the pad at every charge, when an edge lifts, or after 2 days; "take the pad off before charging" on the low-battery prompt; suggests switching sides at each change to rest the skin; counts the pads left on the card (six) | App, battery events | EVT |
| 26 | Wear orientation | Upright, window and antenna end up. The IMU axes come from the U11 placement, so sleep position, activity and motion gating use head axes, not pod axes. The side of the head is the user's choice in the app (default left; labelled "Left temporal" / "Right temporal"); the fit guide suggests the side the user sleeps on less. The app does not try to detect the side | IMU | EVT |
| 27 | Measurement mode | Full, Smart (default) or Lite in the app, each with its expected charge life: Smart about 4 days, Full about 2 and Lite about 6 [ASSUMED until EVT is measured]. Lite reads heart rate in bursts (every 2 min) and gives no HRV | Mode characteristic 0x0601 (`ble_gatt.md`) | EVT |

## App

- **Design:** a clickable prototype with sample data is in `app/android-ios/design/`. Its README maps each screen to the feature numbers above.
- **Build:**
  - Flutter for the UI and the shared app logic: BLE protocol, log decoding, scores, storage.
  - Pigeon for type-safe calls into native code, used where the platform matters: BLE connection and background sync (iOS state restoration, an Android foreground service), firmware update through Nordic's Device Manager libraries (MCUmgr SMP), and HealthKit / Health Connect.
- **Words:**
  - The pod is worn on the "temporal region"; the no-glasses spot is the "temporal hollow". Use only these anatomical terms for the spot, in the app, the docs and commits.
  - Never use another company's product or metric names.
- **Scores:** Recovery (#7), Sleep (#4) and Effort (#6) are Atrium's own 0–100 scores, with documented formulas. Every reading screen carries the wellness line.
- **Imagery:** product images come from the mechanical CAD. Portraits are generated, show no real person, and keep the pod at true scale (20.9 × 11 mm, upright).

## Hardware targets that serve these features

| Target | Value | Why |
|---|---|---|
| Size | 20.9 × 11 × 5.3 mm EVT pod | It is worn on the head, so it should go unnoticed |
| Battery | About 4 days per charge on a 30 mAh cell (3.6–8 simulated; measure in EVT); charged on the Plinth | Size beats battery life on the head |
| Water | Sweat- and shower-resistant [VERIFY: IP rating to set before tooling] | 24/7 wear |
| Radio | BLE 5, KH-1608-H08 chip antenna | |
| Skin attachment | A disposable double-coated pad, about 0.15 mm, hidden under the pod: it sits in a 0.05 mm bed inside a flat frame (the shell rim and a base lip), its one hole is a pill round the optical window, 130 mm² bonded, nothing shows. Six to a card; the pod picks a pad up aligned. Off before charging (it covers the contacts) | The adhesive must not show; the frame shields the pad edge from pillows and glasses arms. Expect 1–2 days per pad [EST: wear test on 5 people] |
| Plinth | Magnetic charging stand: two magnets pull a steel stiffener in the pod onto two spring pins (VIN at the centre, GND either side, so either way round charges) | No cable on the head; the pod drops in aligned |

## Not features

- Medical claims, diagnosis, alerts that imply a condition, or blood-oxygen numbers.
- Brain, cerebral or cognitive measurements.
- Any metric whose method is not Atrium's own and documented.
