# What the temporal-region PPG contains: report, 2026-11-08

**Status: no Atrium head data exists.** This report rests on three sources, never on head measurements:
- the physics prior (the repo's own photon Monte Carlo);
- method checks on public finger PPG with the same three wavelengths;
- simulated studies with known answers.

The hypotheses were fixed first (`hypotheses.md`, 16:22 IST). Every number below says which of the three it comes from.

Atrium is a wellness and research device. Nothing here is a medical measurement. Atrium's optical outputs are **relative optical indices** (pulse amplitude, optical-density change, residuals). They are **scalp-dominated** and **not validated against transcranial Doppler or MRI**.

## What we can claim

1. **At the pod's spacing, the pulse signal is expected to come almost entirely from the scalp.** Source: Monte Carlo, layer thicknesses [ASSUMED].

   | Channel | Scalp | Temporalis muscle | Brain |
   |---|---|---|---|
   | Green | essentially all (> 99.9 %) | | 0 |
   | Red and IR | 88–89 % | 11–12 % | 0.006–0.008 % (6–8 × 10⁻⁵) |

   Spacing is about 2 mm [VERIFY]. Even at 3 mm the brain share is 0.016 %. With these layers, the muscle share is more than 1,000 times the brain share at 2–3 mm, and still about 80 times at 20 mm (`figures/f6_depth.png`).

   **Thinner layers, same conclusion.** MRI puts the adult temporalis at 3.7–17 mm between people (360 adults, *Surgical Neurology International*); the temporal skull is thin, and 2 mm is our assumption. The table above assumes 7 mm and 3 mm. Re-running the Monte Carlo with 5 mm and 2 mm (`data/monte_carlo/s1_mc_thin.json`) gives an IR brain share of:
   - 4.9 × 10⁻⁴ (0.05 %) at 2 mm;
   - about 1 × 10⁻³ with a 3.7 mm temporalis.

   Muscle stays at 11 %, so it is still 110–230 times the brain share. At 30 mm the same model gives a brain share of 10.6 %, inside the 8–13 % that published head-template Monte Carlo finds at 25–35 mm. That agreement supports the model.

2. **The planned CO₂-challenge study cannot detect a brain component of that size.** Source: 500 simulated studies per point (`data/power/`, `figures/f7_power.png`). With every assumption in the simulation, 80 % power needs a brain share of the IR pulse of:
   - 3.8 % with 12 subjects;
   - 2.0 % with 24 subjects.

   Physics predicts 0.008 % with the original layers, so the design is **about 240–450 times short**; at 3 mm it is 220–280 times short. With the published thinner layers (0.05 % at 2 mm) it is **40–80 times short**, and 20–40 times with the thinnest temporalis.

   The research modes do not close this gap. At 100–200 Hz the threshold moves only from 454 to 360 times short (12 subjects), and from 237 to 231 times (24 subjects). A noiseless sensor gets 346 and 231 times. The limit is beat-to-beat variation in scalp and muscle blood volume, which has the same shape as any deeper pulse. It is not the sensor (`research_mode.md`).

3. **A "deeper than green" result would not point to the brain.**
   - In simulation, a temporalis response to CO₂ of 5 % produces a "significant" IR residual beyond green in 48 % of 12-subject studies and 76 % of 24-subject studies, with **no brain signal at all**. At 10 % it does so in 96–100 %.
   - In the full-pipeline run (12 subjects, 25 Hz), the muscle-only scenario gives b = +0.8 % [+0.3, +1.4], p_holm 0.017 (`figures/f3_co2_challenges.png`).
   - The confound is built into the anatomy: the temporalis sits between the scalp and the skull.

4. **A wavelength timing difference is not depth evidence on its own.** Source: real finger data, n = 22, sitting, 500 Hz.
   - IR leads green by a median of 25 ms per beat (IQR 16–36 ms; IR earlier in 21 of 22).
   - The IR pulse arrives 16 ms earlier after the ECG R peak (IQR 10–26 ms).

   There is no brain in a finger, so an IR–green lag at the head cannot by itself be read as an intracranial signal.

5. **What costs timing at 25 Hz is aliasing, not the rate itself.** Source: real finger data, n = 22, sitting (`research_mode.md`, `data/research_mode/`; amended H6).
   - In its everyday modes the firmware reads one short integration per 25 Hz frame (SMP_AVE = 1), so pulse content and noise above 12.5 Hz fold into the band. 95th percentile of |error per beat| against 500 Hz:
     - pulse-arrival time (green): 20 ms;
     - IR–green lag: 8.8 ms.
   - Research mode 7 (200 Hz) brings those to 5 ms and 1.9 ms.
   - Sampling at 200 Hz inside the module and averaging down to 50 Hz does as well as streaming 200 Hz.
   - Rise time and other shape features need 200 Hz and are still coarse there (95th percentile 67 ms).
   - Amplitude and optical-density analyses are fine at 25 Hz.

   An earlier version of this claim gave whole-record error SDs: 5.2 ms (lag), 13.6 ms (pulse arrival) and 40 ms (rise time). Those SDs are set by the 1 % of beats where the foot detector jumps, which happens at every rate. They measured the detector, not the sample rate.

6. **The analysis pipeline gives the right answer when the answer is known.** Source: simulation, 12 subjects, 25 Hz, end-to-end from raw counts.
   - STA compression: recovered red and IR falls of 0.92 of green's [0.90, 0.94]. The model truth is about 0.91.
   - Null: H3 finds nothing (b = −0.1 % [−0.7, +0.5]).
   - The false-positive rate of H3 with no deep signal is **7 %, not the nominal 5 %**, over 2,000 simulated studies at 2 mm.

   Two method faults were found and fixed this way. Both now have regression tests:
   - The first per-beat amplitude estimator (peak minus foot) produced a **false "deep" detection** in the null case (p_holm 5 × 10⁻⁵). Breathing drift and noise biased it nonlinearly on the weakest channel. The fix is a linear template projection.
   - A single global beat threshold split beats in two when the pulse shrank. The fix is an adaptive refractory period. On one real finger record, 603 ECG beats now give 603 feet at 25 Hz.

## What we cannot claim

- **Anything measured about the temporal region.** No head recording exists. H1, H2, H3 and H5 are untested on real tissue. The figures labelled SIMULATED are model output.
- **That Atrium measures or tracks blood flow in the brain**, or any brain, cognitive or intracranial quantity. Physics predicts the component is 10⁻⁴ to 10⁻³ of the signal, depending on layer thickness. No study design available to the pod can see it, at any sample rate.
- **That a red or IR residual beyond green is intracranial.** At most it is "consistent with muscle, subcutis or deeper tissue".
- **Modified Beer–Lambert ΔHbO and ΔHbR as concentrations.** They assume a uniform change along the Monte Carlo's mean path. In layered tissue they are a dermis-weighted mean, so they are relative indices only.
- **That the jaw-artefact detector works.** Its AUC of 1.00 is on simulated artefacts, which are cleaner than real ones. The real value comes from P6.
- **Ambient-light leakage.** It is unknown. The everyday modes have no direct-ambient slot; research modes 6 and 7 add one (`ble_gatt.md` rev 14). They are in the firmware but not bench-tested.
- **The source–detector spacing.** It is unknown to better than about ±0.5 mm [VERIFY].

## What would change our mind

- **The scalp prior is wrong** if, in P2, IR or red falls by less than half of green's fall in most subjects (H1 falsified). That would mean a large share of the red and IR pulse comes from tissue the STA does not feed. That share would be muscle first; testing it needs a muscle-only manoeuvre (jaw clench at rest, P6).
- **A deeper component exists** if three things hold together:
  - H3 shows b ≠ 0 after Holm correction;
  - b survives regression on HR, the finger reference PPG and posture (method C);
  - the same b does **not** appear with a temporalis-only manoeuvre.

  Even then the wording is "consistent with a component deeper than the scalp".
- **A brain-specific component** would need, at minimum, all of the following:
  - a spacing of 30 mm or more (not the pod: it is 20.9 mm long);
  - a short-separation channel for regression;
  - a simultaneous transcranial Doppler reference;
  - agreement in direction across P3 and P4, with no response to P2, P7 or jaw clench.

  Nothing in this repo can do that today.
- **The Monte Carlo layers** (3 mm scalp, 7 mm or 5 mm muscle, 3 mm or 2 mm skull) are [ASSUMED] for any one person. Thinner layers raise the deep shares:
  - the photon model re-runs with any layer thickness (outputs in `data/monte_carlo/`);
  - measured thicknesses (ultrasound at the spot, 5 people, E5) would fix claim 1 per person.

  Even the thinnest published temporalis (3.7 mm) leaves the brain share about 20 times under the detection limit at 24 subjects.

## Next experiments, with minimum subject counts

| # | Experiment | Needs | Minimum n | Why that n |
|---|---|---|---|---|
| E1 | Firmware research mode: green, red, IR and direct-ambient at 100–200 Hz; IMU at 104 Hz | firmware change (owner) | bench, 3 | Check the stream is gapless and the drift is below 50 ppm |
| E2 | Scalp dissociation: P1, P2, P7 on the pod | EVT pod, research mode | 10 | H1's effect is large (simulated ratio 0.92, between-subject SD about 0.03); 10 gives the passing share to ±15 % |
| E3 | CO₂ challenges: P3, P4, P5 with capnometer, chest ECG and finger PPG | E2 setup plus references | 24 | Gives 76 % power to see a 5 % muscle-type response (simulation); fewer cannot separate it from noise |
| E4 | Jaw library: P6 at rest plus 2 days of normal wear, IMU at 104 Hz | E1 | 10 | Leave-one-subject-out AUC to about ±0.05; the daytime artefact share with a CI |
| E5 | Layer thickness at both spots (ultrasound) | clinic access | 5 | Replaces the [ASSUMED] layers in the Monte Carlo |
| E6 | Two-distance research probe (not the pod): 8 mm short channel, 30–35 mm long channel, with transcranial Doppler | neurology lab collaboration | 20 | The only route to a real deep comparison; only worth doing if E3 shows a residual that survives the muscle control |

E2 and E3 can share sessions. E6 is out of scope for the pod and is listed so the limit is explicit.

## Data

Every number above comes from the files in `data/` and the figures in `figures/`. The analysis code is not part of this
release.
