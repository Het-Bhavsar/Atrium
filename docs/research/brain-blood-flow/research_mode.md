# Research mode: what 100–200 Hz adds, and where it stops (2026-11-08)

**Status.** Firmware research modes 6 (100 Hz) and 7 (200 Hz) stream green, IR, red and direct ambient, with the IMU at 104 Hz (`ble_gatt.md` rev 14). They are uncommitted in the firmware worktree and not bench-tested. No head recording exists.

Every number below names its source:
- **finger** is real data: pulse-transit-time-ppg 1.1.0, 22 adults sitting, MAX30101 at 500 Hz, with ECG.
- **MC** is the repo's photon Monte Carlo.
- **sim** is simulated studies.

The results are in `data/research_mode/`.

## The answer: what the pod can reach

| Level | What | Released firmware (25 Hz) | Research mode (100–200 Hz) | What it would take otherwise |
|---|---|---|---|---|
| 1 | Pulse rate, beat intervals | yes | yes | |
| 2 | Scalp perfusion index and other relative optical indices; trends; posture and breathing effects on them | yes | yes | |
| 3 | Pulse timing: arrival time after a chest-strap ECG R peak, IR–green lag | coarse (95th percentile error 20 ms and 8.8 ms) | **good** (5 ms and 1.9 ms) | |
| 4 | Pulse shape: rise time, notch, reflection ratios | no | coarse (95th percentile rise-time error 67 ms at 200 Hz) | 500 Hz or more |
| 5 | Response beyond the scalp to breathing challenges | "consistent with" muscle or subcutis only | unchanged | a muscle-only control (jaw clench) |
| 6 | A deep (intracranial) haemodynamic change | **no**: 40–450 times under the detection limit | **no**: unchanged | separate source and detectors at 25–35 mm plus an 8 mm short channel (fNIRS) |
| 7 | Blood flow in the brain | **no** | **no** | coherent laser and photon-counting detectors at 25–30 mm (DCS or speckle contrast), or transcranial Doppler ultrasound |

The pod's ceiling is **levels 1–3**, with level 3 needing research mode. Levels 6 and 7 are not reachable with a single module whose LEDs and photodiode sit about 2 mm apart [VERIFY], at any sample rate. They are not reachable in the pod's 20.9 mm body either (below).

## 1. What research mode buys: pulse timing (finger, real data)

The released firmware reads one short integration per 25 Hz frame (SMP_AVE = 1, `maxm86161_regs.c`). Nothing filters the optical signal before it is sampled, so pulse content and noise above 12.5 Hz fold back into the band. That folding, not the 25 Hz rate itself, is what costs timing:
- Ideal anti-aliased decimation to 25 Hz keeps per-beat timing within 2 ms (95th percentile).
- Point sampling at 25 Hz does not.

Error per beat against 500 Hz, in ms, as the median / 95th percentile of the absolute error:

| How the module samples | IR–green lag | Pulse arrival, green | Pulse arrival, IR | Rise time, green |
|---|---|---|---|---|
| 25 Hz, one integration per frame (**released firmware**) | 2.2 / 8.8 | 5.0 / 20.0 | 2.0 / 13.1 | 25 / 124 |
| 50 Hz, one integration per frame | 1.5 / 6.2 | 3.0 / 15.0 | 2.0 / 8.0 | 19 / 123 |
| 100 Hz, one integration per frame (mode 6) | 1.0 / 4.4 | 3.0 / 10.0 | 2.0 / 6.0 | 15 / 109 |
| 200 Hz, one integration per frame (mode 7) | 0.4 / 1.9 | 1.0 / 5.0 | 0.0 / 2.0 | 4 / 67 |
| 200 Hz inside the module, averaged to 25 Hz | 0.6 / 2.5 | 2.0 / 7.0 | 2.0 / 6.0 | 16 / 65 |
| 200 Hz inside the module, averaged to 50 Hz | 0.5 / 2.1 | 2.0 / 5.0 | 0.0 / 3.0 | 10 / 62 |
| Ideal anti-aliased 25 Hz (lower bound, not buildable) | 0.1 / 0.4 | 0.0 / 2.0 | 0.0 / 2.0 | 12 / 46 |

Medians over 22 sitting records. Pulse-arrival errors come in 2 ms steps, because the foot is a 500 Hz sample index.

**What follows from the table:**
- For timing, **sampling fast matters more than streaming fast.** 200 Hz averaged to 50 Hz matches streaming 200 Hz for lag and pulse arrival, at a quarter of the BLE data. 200 Hz averaged to 25 Hz gets most of the way there.
- Firmware modes 8 and 9 (`ble_gatt.md` rev 15) do exactly this: they sample at 200 Hz with on-chip averaging (SMP_AVE 4 or 8) and stream 50 or 25 Hz. The LED fires as often as in mode 7, so their LED current is close to mode 7's, not to today's 25 Hz modes [ASSUMED until measured].
- The research modes already average down to 25 Hz for the on-pod filters (`modes.h`), so the pod's own pulse and HRV filters get the anti-aliasing benefit in those modes.
- **Shape features stay coarse.** Rise time has a 95th percentile error of 67 ms even at 200 Hz. Morphology claims need 500 Hz or more, or a model-based fit.

**Caveats:**
- How much folds back depends on the sensor's own noise above 12.5 Hz. The finger set uses a MAX30101, not the pod. That the pod behaves the same way is [ASSUMED].
- **Bench check (E1):** record mode 7 on the pod, decimate it in software both ways, and compare.
- The whole-record error SDs in the first version of the report (5.2, 13.6 and 40 ms) were set by the 1 % of beats where the foot detector jumps. That happens at every rate, so those SDs measured the detector, not the rate. They are corrected in `report.md` and amended in `hypotheses.md`.

## 2. What research mode does not buy: depth (MC and sim)

**Where the red and IR pulse comes from** (MC, IR, medium skin, nominal blood):

| Spacing | Brain, 7 mm muscle / 3 mm skull | Brain, 5 mm / 2 mm (published typical) | Temporalis muscle | Scalp |
|---|---|---|---|---|
| 2 mm (the pod, [VERIFY]) | 0.008 % | 0.05 % | 11 % | 89 % |
| 3 mm | 0.016 % | 0.10 % | 21 % | 79 % |
| 8 mm (a short channel) | 0.09 % | 0.56 % | 58–60 % | 39 % |
| 18 mm (the most the pod body could fit) | 0.66 % | 3.2 % | 72–77 % | 19 % |
| 30 mm (fNIRS-style) | 3.2 % | 10.6 % | 65–77 % | 12 % |

The muscle and scalp columns cover both layer sets.

With a 3.7 mm temporalis, the thinnest published, the 2 mm brain share is about 0.1 %. The 30 mm, 5 mm-muscle value (10.6 %) sits inside the 8–13 % that published head-template Monte Carlo gives at 25–35 mm. Two points follow:
- **At the temporal region the temporalis is always larger than the brain.** At 30 mm it is still 65–77 % of the pulse. This site is harder than the forehead, where fNIRS is usually done.
- **Even a redesigned pod with separate LEDs and photodiodes 18 mm apart would see about 1–3 % brain under 72–77 % muscle.** That puts it below the CO₂ study's detection limit.

**Detection limit at each sample rate** (sim, H3 CO₂ test, 500 simulated studies per point, 2 mm). The column headings are subject counts.

| Sample rate | Brain share needed for 80 % power, 12 | 24 | Times short of physics (7 mm / 3 mm), 12 | 24 |
|---|---|---|---|---|
| 25 Hz | 3.8 % | 2.0 % | 454 | 237 |
| 100 Hz | 3.0 % | 1.9 % | 360 | 233 |
| 200 Hz | 3.0 % | 1.9 % | 360 | 231 |
| noiseless sensor | 2.9 % | 1.9 % | 346 | 231 |

With the published thinner layers the gap is 40–80 times, and 20–40 times with the thinnest temporalis.

**Why sample rate cannot fix this:**
- The limit is the window-to-window variation of scalp and muscle blood volume (3 % SD in the simulation, [ASSUMED]). It has the same pulse shape as any deeper component, and one spacing gives no second view to separate them.
- Sensor noise on its own is not the problem. At 200 Hz the ADC noise on one beat's amplitude is about 1.8 counts, while a 2 mm brain component is about 0.03 counts of a 390-count IR pulse. Seeing it at a signal-to-noise ratio of 3 would take about 28,000 beats (7 h) of a perfectly stable scalp.
- The scalp is not stable. On the finger data, beat-to-beat amplitude varies by about 4.5 % of the pulse. That is 90–550 times the whole brain component (0.008–0.05 %), with the same shape.

## 3. How optical measurement of the brain's blood supply works (published practice)

So that Atrium's wording can be placed correctly:

- **Continuous-wave near-infrared spectroscopy (fNIRS).**
  - **What it measures:** changes in oxy- and deoxy-haemoglobin with the modified Beer–Lambert law. The light path is the separation times a differential pathlength factor, about 6 in adults on the forehead.
  - **Where the sources and detectors sit:** 25–35 mm apart, where the brain's share of the path is about 8–13 % on a head template. Brain sensitivity is still biased to the outer 10–15 mm of the head.
  - **Removing the scalp:** a short channel at about 8 mm is regressed out. Doing so improves contrast-to-noise by about 50 % for oxy-haemoglobin in published tests.
  - **At the temporal region:** jaw clenching produces large haemoglobin-like artefacts on 30 mm channels, and a 10 mm short channel did not reliably remove them.
- **Diffuse correlation spectroscopy (DCS).**
  - **What it measures:** a blood-flow index, fitted from the speckle intensity autocorrelation, which needs a long-coherence laser and single-photon detectors.
  - **Where it works:** typically 25 mm on the adult head, 30 mm at most. Brain sensitivity there is about 20 %, and even at 30 mm DCS is more sensitive to scalp than brain.
  - **Validation:** against arterial-spin-label MRI, PET and transcranial Doppler.
  - **Cheaper relatives:** speckle contrast (SCOS) and interferometric DWS use cameras, still at large separations.
- **Transcranial Doppler.** Ultrasound that measures blood **velocity** in the middle cerebral artery, at 45–60 mm depth, through the thin temporal bone.
- **What the pod's module is.** One photodiode and three LEDs about 2 mm apart, with incoherent LEDs and no ultrasound. It is a photoplethysmograph: it measures the scalp's blood-volume pulse. The superficial temporal artery under it branches from the external carotid and feeds the scalp. The brain is fed by the internal carotid and vertebral arteries.

**Main published sources:**
- fNIRS depth sensitivity on a head template: Strangman, Li and Zhang, PLoS ONE 2013.
- Differential pathlength factor: Scholkmann and Wolf, J Biomed Opt 2013.
- Optimal short-channel distance: Brigadoi and Cooper, Neurophotonics 2015.
- Short-channel regression: Gagnon et al., NeuroImage 2012.
- Temporalis artefacts in fNIRS: Schecklmann et al., Front Hum Neurosci 2017.
- DCS review: Carp, Robinson and Franceschini, Neurophotonics 2023.
- DCS vs NIRS brain sensitivity: Selb et al., Neurophotonics 2014.
- Transcranial Doppler: a methods review on PubMed Central, PMC3876587.
- Temporalis and skull thickness from MRI and CT cohorts (2020–2022).

## 4. Wording this supports

- **Allowed:**
  - "pulse rate";
  - "pulse arrival time" (with a chest strap);
  - "a relative optical index of blood volume at the temporal region, scalp-dominated, not validated against transcranial Doppler or MRI";
  - "consistent with changes in scalp or muscle perfusion".
- **Not allowed:** any of these words, in any form: "brain blood flow", "cerebral blood flow", "cerebral perfusion", "brain oxygen", "measures the brain".
- **No study the pod can run supports them, at any sample rate.**

## Next steps this adds

- **E1 (already listed):** bench-test modes 6 and 7, and compare point-sampled against averaged on the pod's own signal. Minimum 3 pods, 10 min each.
- **E1b:** firmware modes 8 and 9 now do this (`ble_gatt.md` rev 15): they sample at 200 Hz and average on the module to 50 Hz (SMP_AVE 4) or 25 Hz (SMP_AVE 8). They are uncommitted and not bench-tested, and their currents are [ASSUMED]. `capture.py` decodes them and moves each word to its window centre (7.5 or 17.5 ms earlier). On the bench, check SMP_AVE reads back from 0x12, and measure the current.
- **E5 (already listed):** ultrasound layer thickness at both spots, 5 people. This now matters more, because the plausible range of the 2 mm brain share spans a factor of 12 (0.008–0.1 %).
