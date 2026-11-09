# Pre-registered hypotheses: what the temporal-region PPG contains

**Written 2026-11-08 16:22 IST, before any analysis of head recordings.** No Atrium head recordings exist on this date (see `data_inventory.md`).

Edits after head data exists go at the end, under "Amendments", with their own date. Nothing above that line may change.

## Prior from physics (stated before data)

The repo's layered Monte Carlo (4 × 10⁷ photons, medium skin, nominal blood) gives the share of the detected light's path in each layer. Its layers are:
- 3 mm scalp;
- 7 mm temporalis muscle;
- 3 mm skull;
- 2 mm CSF;
- brain.

All thicknesses are [ASSUMED]. Values at a 2 mm source–detector spacing:

| Wavelength | Skin (dermis) | Subcutis | Muscle | Skull | Brain |
|---|---|---|---|---|---|
| 530 nm (green) | 97.8 % | 2.1 % | 0.0 % | 0 | 0 |
| 660 nm (red) | 78.6 % | 16.0 % | 5.3 % | 0.05 % | 0.003 % |
| 830 / 940 nm (bracketing IR 880) | 77–80 % | 16 % | 4–6 % | 0.02–0.08 % | 0.001–0.006 % |

The MAXM86161 source–detector spacing is not dimensioned in the data sheet. From the 2.9 × 4.3 mm package it is about 1.5–2.5 mm [VERIFY against the F142A4+1 package drawing]. At 3 mm the brain share is still 0.001–0.012 %.

So the expected brain share of the red and IR signal is about 10⁻⁵ to 10⁻⁴. The muscle share is about 10³ times larger. Any "deeper than green" signal is expected to be **muscle and subcutis first**, not intracranial.

## Primary hypotheses

The family is H1–H3. α = 0.05 with Holm correction across the three.

**H1 (scalp dominance).** During superficial temporal artery compression (P2), pulse amplitude falls on all three channels.
- Expected: in at least 70 % of subjects, green, red and IR each fall by at least 30 % of baseline, and the red and IR falls are at least half of green's fall.
- Falsified if red or IR consistently falls by less than half of green's fall. That would mean a large share of the red and IR pulse comes from vessels the STA does not feed.

**H2 (green explains IR).** At seated rest (P1), the green waveform explains most of the IR waveform.
- Pulsatile band (0.5–5 Hz), 8 s windows with β fitted per window: median R² of IR on green ≥ 0.80 across subjects.
- Slow band (0.01–0.1 Hz) optical density: median R² ≥ 0.60.
- Falsified if the median R² is below these values. Note that a low R² can also come from noise; the per-subject SNR is reported next to it.

**H3 (no detectable CO₂-specific component beyond scalp).** For each subject, fit relative pulse-amplitude change per challenge window as

`r_c = a · r_green + b · x_CO2 + e`

where x_CO2 is +1 for breath-hold (P4), −1 for hyperventilation (P3), and 0 for STA compression (P2), cold pack (P7) and posture (P5).

Expected: the group mean of b for IR and for red has a 95 % CI that includes 0.

If b is reliably non-zero, the result is reported as **"a CO₂-linked component beyond what green explains, consistent with muscle, subcutis or deeper tissue"**. It is never reported as intracranial, because muscle and skin vessels also respond to CO₂ and to the blood-pressure change of both challenges.

## Secondary hypotheses (reported, not part of the primary family)

- **H4 (systemic).** P3 and P4 change HR and pulse amplitude on every channel. Where a finger or earlobe reference PPG was recorded, its pulse-amplitude change correlates with each Atrium channel's at r ≥ 0.7 across challenge windows.
- **H5 (jaw artefacts).** An IMU-plus-PPG detector separates chewing, talking and jaw-clench windows (P6) from rest (P1) with leave-one-subject-out AUC ≥ 0.80. The share of daytime wear these artefacts cover is estimated, with no prediction.
- **H6 (sample rate).** At 25 Hz, the pod's current rate, per-beat IR–green lag and pulse-arrival time have an error SD larger than 5 ms against the same beats at 500 Hz. Tested now on the public finger dataset by decimation, because no head data exists.

## Exploratory (FDR 10 %, Benjamini–Hochberg across features × channels)

- Morphology per beat and channel: rise time, 50 % width, notch timing, H2/H1 and H3/H1, IR–green lag, and pulse-arrival time where ECG exists. Does any feature separate P3/P4 from P2/P7 better than amplitude alone?
- Modified Beer–Lambert ΔHbO and ΔHbR from red and IR, relative only. Path lengths come from the Monte Carlo at 2 mm, not from a literature DPF.
- Leave-one-subject-out classifiers (ridge or logistic only) for P3 vs P4 vs P2 windows, and EtCO₂ change where a capnometer exists.

## Fixed analysis choices

- **Windows, relative to challenge onset:**

  | Window | Span |
  |---|---|
  | Baseline (all challenges) | −30 to 0 s |
  | P2 response | 5–15 s into compression |
  | P3 response | last 20 s of hyperventilation |
  | P4 response | last 10 s of the hold plus 10 s after |
  | P7 response | 30–60 s |
  | P5 response | 30–90 s after the change |

- **Beat timing:** from ECG R peaks when present, otherwise from green pulse feet. Every channel uses the same beats.
- **Exclusions, fixed now:**
  - windows with IMU motion above 50 mg RMS (0.5–8 Hz), excluding protocol P8;
  - windows with any sample within 1 % of ADC full scale;
  - windows with perfusion index outside 0.02–5 %;
  - a subject is dropped from a protocol with fewer than 2 of 3 usable repeats.

  Each exclusion is counted and reported.
- **Lag searches:** every lagged correlation is tested against at least 1,000 phase-randomised surrogates and 1,000 block-shuffled surrogates (block = 10 s). Both null distributions are reported.
- **Reporting:** every subject is shown, as well as the mean. Effects are within-subject, with 95 % CIs.
- **Cross-validation:** leave-one-subject-out only. No window-level random splits.

## Amendments

**2026-11-08, H6 (before any head data).** The pre-registered test (error SD > 5 ms at 25 Hz) cannot separate the sample rate from the detector. About 1 % of beats move to another valley of the foot search, and that happens at the same rate whether the signal is resampled to 25 Hz or 250 Hz. The SD is set by those beats: pulse-arrival SD is 13.0 ms at 25 Hz and 11.3 ms at 200 Hz on the same records. The original test and its result stay as stated above.

Added analysis, labelled post hoc:
- **Metrics:** the median and 95th percentile of |per-beat error| against 500 Hz, and the share of beats with an error over 5 ms.
- **Sampling cases:** three ways of reaching each rate:
  - ideal anti-aliased decimation;
  - point sampling with no anti-alias filter, which is how the module samples with SMP_AVE = 1;
  - 200 Hz point sampling averaged down on the chip.

The point-sampled case is the one that describes the everyday modes.
