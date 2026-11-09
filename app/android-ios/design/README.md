# App design

`prototype/index.html` is a clickable prototype of the Atrium phone app with sample data. Open it in a browser. The panel beside the phone jumps to any screen and sets:
- the build (EVT or V1)
- the theme and the time of day
- the pod and Plinth state and the data state
- notifications

## Build

The app will be Flutter, with Pigeon for the native parts: BLE and background sync, firmware update (Nordic Device Manager, MCUmgr SMP), and HealthKit / Health Connect. The split is in `docs/FEATURES.md` (App). Work starts after the design is approved.

## Direction

- **Open to the sky.** Today looks up through an atrium's open roof. The sky follows the time of day. Detail pages use the same language: the page's photo as a cover, the reading on it, one sentence under it.
- **Light is the only colour in the data.** Green for heart rate, HRV and Recovery; infrared, drawn as violet, for sleep; red for Effort and warmth.
- **Score gauge.** The hall outline from the Atrium mark, filled along its length.
- **Type.** Newsreader (serif) for headings and numbers; the system sans for everything else.
- **Product imagery.** The EVT pod is a capsule with a flat top and an R 1.8 mm round, in silver-grey metallic. Every page has its own photo: a different person, place and pose, in colour daylight (night for Sleep). The pod is generated in the photo from the CAD render as a reference, then each photo is checked for scale against the ear and the eye-to-ear distance (pod 20.9 × 11 mm), worn upright. The skin pad never shows.

## Screens and features

Feature numbers and status come from `docs/FEATURES.md`.

| Screen | Features | Status |
|---|---|---|
| Welcome, wellness notice | #23 | EVT |
| Bluetooth, lift off the Plinth, find and pair, pairing | #15, #17, #21 | EVT |
| Glasses question; fit and side; signal bar, then press 10 s | #24, #25, #26, #1, #18 | EVT |
| Baseline (three nights) | #7 | V1 |
| Today | #1, #3; adds #13 in V1 | EVT / V1 |
| Recovery, with its contributors | #7 | V1 |
| Heart; HRV section | #1; #2 | EVT; V1 |
| Sleep: stages, head position while asleep, time lying on the pod, overnight | #4, #5 | V1 |
| Effort: zones, workouts, active minutes | #6 | V1 |
| Skin temperature | #3 | EVT |
| Device: battery, measurement mode, Plinth charging, cold pause, skin contact, wear side, readings held, storage off | #16–#20, #26, #27 | EVT |
| Skin pad: change steps, pads left, switch sides | #25 | EVT |
| Pod software update | #21 | EVT |
| Settings: export | #22 | V1 |
| Settings: no account, local data, delete | #23 | EVT |
| Settings → Labs: oxygen trend, thermal and perfusion trends, eating prompt, cycle view, assistant | #8–#12, #14 | Later, off by default |

## Numbers

All readings are sample data. Hardware is the EVT pod: 20.9 × 11 × 5.3 mm, 30 mAh cell, charged on the Plinth.

**Battery.** 30 mAh × 95 % usable over the average day-and-night draw. Smart at 0.30 mA gives about 4 days, matching `docs/FEATURES.md` (3.6–8 days simulated). The Full and Lite draws are [ASSUMED] until EVT is measured. Battery % is shown in 10 % steps.

| Mode | Average draw | Full charge lasts |
|---|---|---|
| Full | 0.6 mA [ASSUMED] | about 2 days |
| Smart | 0.3 mA | about 4 days |
| Lite | 0.2 mA [ASSUMED] | about 6 days; heart rate in bursts, no HRV |

**Charging.** The pad comes off first, because it covers the contacts. The pod then sits on the Plinth either way round. Charging runs between about 0 and 45 °C and is slower when cold.

**Sync.** The pod holds about 16–22 h of readings, so the app syncs whenever it's near and warns after 12 h without a sync.

**Turn off for storage.** Only on the Plinth: tap Turn off, then lift the pod. It wakes on the Plinth.

**Skin pad.** Change it at every charge, when an edge lifts, or after 2 days; six to a card. The app counts pads, reminds at 48 h and suggests the other temporal region each time.

**Fit.**
- No glasses: the hollow above the cheekbone, a thumb-width behind the outer eye corner.
- Glasses: higher, at eyebrow level, with the arm just under the pod.
- Upright, window end up. The live signal bar then turns green, and you press for 10 s.

## Images

The images in `prototype/img/` were made for Atrium.
- **Portraits:** generated with GPT Image 2.6; they show no real person.
- **Pod, Plinth and packaging:** rendered in Blender from the mechanical CAD.

Licence: CC BY-SA 4.0, like the other docs and images.

Wellness and research only. No screen makes a medical claim.
