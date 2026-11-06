# Atrium features

The single list of what Atrium does, for hardware, firmware, app and cloud work. A feature is built only once it is listed here. Change it by pull request; every workstream reads it.

Atrium is a wellness and research device. Every feature below gives wellness trends, never a diagnosis, treatment or medical measurement (see `certification.md`).

**Status:**
- **EVT:** the first build must support it end to end.
- **V1:** first public release.
- **Later:** a research item, built after its signal is validated on the bench.

Sensors are the Rev B pod's: PPG (green, red and IR, near detector), IMU (LSM6DSV16X), skin temperature (TMP117), an edge NTC, a fuel gauge, BLE (nRF52832), and charge and dock state. The GATT interface is in `ble_gatt.md`.

## Body metrics

| # | Feature | What the user sees | Source | Status | Notes |
|---|---|---|---|---|---|
| 1 | Heart rate | Live and 24 h HR, resting HR, HR zones in workouts | PPG green, 1 Hz output | EVT | Validate against a chest strap during treadmill steps, light skin to dark skin, and publish the result |
| 2 | HRV | Nightly RMSSD and its trend | PPG beat-to-beat, at rest and asleep | V1 | Only in still, low-motion windows |
| 3 | Skin temperature | Nightly deviation from the user's own baseline | TMP117 | EVT | Relative trend, not body temperature |
| 4 | Sleep | Duration, timing, an estimate of stages, wake-ups | HR, HRV, IMU | V1 | Stages are an estimate; label them that way |
| 5 | Sleep position | Time on the back, left and right sides | IMU orientation | V1 | Head orientation from the accelerometer |
| 6 | Activity and effort | Active minutes, workouts detected, an effort score from HR zones | IMU, HR | V1 | Atrium's own effort score, with the formula documented in `app/` |
| 7 | Recovery | A morning score from resting HR, HRV, sleep and temperature deviation | 1–4 | V1 | Formula documented and versioned |
| 8 | SpO₂ trend | Overnight relative oxygen-saturation trend | PPG red and IR | Later | Uncalibrated, so trend only. Never a % SpO₂ number |
| 9 | Thermal trends | Skin-temperature and pod-gradient trends over the day and during exercise | TMP117, edge NTC | Later | Atrium's own physiology; no composite "score" until validated |
| 10 | Perfusion trend | Relative PPG amplitude (AC/DC) trend | PPG | Later | No brain or cerebral claims (see the simulation notes) |
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
| 16 | Battery and charging | Pod %, charging, docked, full; cold or hot "charging paused" | Fuel gauge, CHG#, VBUS_SENSE, charge policy | EVT |
| 17 | Case status | Case estimate from top-up history; "charge your case" when docking gives no charge | VBUS_SENSE, logic | EVT |
| 18 | Wear detection | Off-body, so pause measurement and save power | PPG DC, TMP117 | EVT |
| 19 | Ship mode | Store and ship with the cell disconnected; wakes on the dock | LPB1010 SHIP | EVT |
| 20 | Log sync | Offline log on the pod (60 days target) and sync when the phone is near | Flash, GATT log service | EVT |
| 21 | Firmware update | Over the air, signed, with rollback | MCUmgr SMP | EVT |
| 22 | Data export and delete | CSV export of the user's data; delete everything | App, cloud | V1 |
| 23 | Privacy | Works without an account. Cloud features are opt-in. No sale or sharing of health data | App, cloud | EVT |

## Hardware targets that serve these features

| Target | Value | Why |
|---|---|---|
| Size | As small and light as the parts allow; Rev B.2 compact study in progress | It is worn on the head, so it should go unnoticed |
| Battery | About 1.5–2 days per charge on the compact cell; the case tops it up | Size beats battery life on the head |
| Water | Sweat- and shower-resistant [VERIFY: IP rating to set before tooling] | 24/7 wear |
| Radio | BLE 5, RFECA3216 chip antenna | |

## Not features

- Medical claims, diagnosis, alerts that imply a condition, or blood-oxygen numbers.
- Brain, cerebral or cognitive measurements.
- Any metric whose method is not Atrium's own and documented.
