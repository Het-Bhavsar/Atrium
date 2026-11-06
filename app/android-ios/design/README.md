# App design

`prototype/index.html` is a clickable prototype of the Atrium phone app with sample data. Open it in a browser. The panel beside the phone jumps to any screen and sets:
- the build (EVT or V1)
- the theme and the time of day
- the pod and case state and the data state
- notifications

## Direction

- **Open to the sky.** The Today screen looks up through an atrium's open roof. The sky follows the time of day: dawn, day, dusk, night.
- **Light is the only colour in the data.** The pod measures with three lights, and each metric takes one:
  - green (530 nm): heart rate, HRV, Recovery
  - infrared (940 nm), drawn as violet: sleep
  - red (655 nm): Effort and warmth
- **The score gauge is the hall outline from the Atrium mark.** An arch on two legs, filled along its length.
- **Type.** Newsreader (serif) for headings and numbers; the system sans for everything else.
- **Themes.** Light is sunlit stone. Dark is the hall at night.

## Screens and features

Feature numbers and status come from `docs/FEATURES.md`.
- **EVT build:** only EVT features. Today shows live and resting heart rate, skin temperature and the device.
- **V1 build:** adds the scores.
- **Later features:** placeholders under Settings → Labs, off by default.

| Screen | Features | Status |
|---|---|---|
| Welcome, wellness notice | #23 | EVT |
| Bluetooth, wake the pod, find and pair, pairing | #15, #19, #21 | EVT |
| Fit, signal check | #1, #18 | EVT |
| Baseline (three nights) | #7 | V1 |
| Today | #1, #3; adds #13 in V1 | EVT / V1 |
| Recovery with its contributors | #7 | V1 |
| Heart; HRV section | #1; #2 | EVT; V1 |
| Sleep: stages, position, overnight | #4, #5 | V1 |
| Effort: zones, workouts, active minutes | #6 | V1 |
| Skin temperature | #3 | EVT |
| Device: battery, charging, cold charging, case, skin contact, storage, ship mode | #16–#20 | EVT |
| Pod software update | #21 | EVT |
| Settings: export | #22 | V1 |
| Settings: no account, local data, delete | #23 | EVT |
| Settings → Labs: oxygen trend, thermal and perfusion trends, eating prompt, cycle view, assistant | #8–#12, #14 | Later, off by default |

**Steps are not shown.** The pod doesn't log them (`docs/ble_gatt.md` rev 3 [VERIFY pedometer at the temple]), so Effort uses heart rate zones, active minutes and detected workouts.

## Numbers

All readings are sample data. Hardware is the Rev B.2 compact pod (`phase2/REV_B2_COMPACT.md` in the design package): 19 × 13 × 5.5 mm, a 30 mAh cell, 15 mA charge, 2 pogo pads, and one optical module.

**Battery life** is 30 mAh × 95 % usable, divided by the average day-and-night draw:

| Mode | Average draw | Full charge lasts |
|---|---|---|
| Full | 1.2 mA [ASSUMED] | about 1 day |
| Smart | 0.6 mA | about 2 days |
| Lite | 0.38 mA | about 3 days |

**The case has no radio, so its level is an estimate.**
- While docked, the app adds up the pod's rise in state of charge × 30 mAh.
- It counts that against a 300 mAh case cell at 90 % usable and 1.3 case mAh per pod mAh: about 7 pod charges.
- The count resets when the case is plugged in.

**"The case ran out"** is decided by the app (`docs/ble_gatt.md` §3.1): the pod reports undocked while it is off-body and still, inside the usual dock window.

**Cold charging** follows the pod's charge states. Below 10 °C the pod charges slowly; below 0 °C or above 45 °C it pauses.

**Storage.** The pod log holds about 63 days at the default 15 s record interval (`docs/ble_gatt.md` rev 3). When the pod is off the skin, it pauses measuring (pod state 3).

## Images

The photographs in `prototype/img/` were generated for Atrium (GPT Image 2.6) and show no real person or product. Licence: CC BY-SA 4.0, like the other docs and images.

Wellness and research only. No screen makes a medical claim.
