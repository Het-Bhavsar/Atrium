# Stroke volume from the temporal region?

Research note, 2026-11-10. Question: can the impedance method in Analog Devices' article "Integrated AFE for
Noninvasive Stroke Volume Monitoring" (Tian Zhu and Elvin Chen, April 2026) help a pod worn at the temporal region?

**Short answer: no.** The method needs a current driven through the chest between electrodes that span the thorax, and
an impedance front end. The pod has neither, and no change to its firmware, its optics or its two skin pads can stand
in for them. A PPG-only or machine-learning stroke-volume number from the temporal region would not be credible either.
Atrium makes no stroke-volume, cardiac-output or haemodynamic claim.

## What the article measures, and with what

- **Quantity.** Stroke volume (SV) and left ventricular ejection time (LVET), by impedance cardiography (ICG). In the
  article's words, ICG "measures stroke volume (SV) by tracking changes in the instantaneous mean thoracic impedance".
- **Front end.** The MAX30009 bioimpedance AFE: a sinusoidal current source (16 Hz to 500 kHz, 16 nA to 1.28 mA rms),
  bipolar or tetrapolar electrode mux, two 20-bit ADCs for I/Q demodulation. It is run with the MAX20356 PMIC and the
  MAX32666 Bluetooth MCU. The authors apply 65.536 kHz at 256 µA.
- **Electrodes.** Four 3M Ag/AgCl gel electrode sites in the standard tetrapolar ICG layout:
  - current is injected at D1, behind both ears, and returns at D4, on the upper abdomen;
  - voltage is sensed between D2, above both clavicles, and D3, either side below the xiphoid.
- **Signal processing.** 50 Hz notch, median and low-pass filters on the impedance, then dZ/dt. The C point is
  (dZ/dt)max. The B and X points bound LVET.
- **Formula.** Their Equation 1 is the Sramek–Bernstein form:
  SV = (W / IW) × (0.17 × H)³ / (4.25 × Z₀) × (dZ/dt)max × LVET.
  - W is body weight and H is height. The 0.17 × H term stands for the thoracic length.
  - IW is ideal weight: 0.536 × H − 17.36 for men, − 27.36 for women.
  - Z₀ is the baseline impedance of the thoracic segment.
- **Validation.** None in the article. It reports no accuracy, reference method or subject count; it shows raw
  impedance and detected B, C and X points. No PPG, no machine learning, and no head-only location: the behind-the-ear
  electrodes are only the current source, with the sensing pair on the chest.

## Against what the pod has

| Pod part | Can it do any of ICG? | Why |
|---|---|---|
| MAXM86161A (green, red, IR, one photodiode) | No | It is optical. It has no current source, no impedance channel and no electrodes |
| LSM6DSOX accelerometer | Not ICG; see "Head ballistocardiography" below | It measures motion, not impedance |
| Two skin NTCs | No | Temperature only |
| VIN and GND skin pads | No | See below |

- **The VIN and GND pads are the charging contacts.** VIN goes straight to the BQ21061 input, which blocks reverse
  current, so the pad is dead on battery (`architecture.md`, power tree).
- Driving 65 kHz through them would need a bioimpedance AFE, isolation from the charger path, and ESD and leakage rules
  for an applied part.
- Even then they are two pads millimetres apart on one pod at the scalp. They would measure the local skin and scalp
  impedance between them, not the thoracic segment.
- The formula needs the thorax's own Z₀ and dZ/dt, measured over a known length (the 0.17 × H term). A head-only pair
  has no path through the chest, so no geometry turns its reading into stroke volume.

**What would need new hardware:** a bioimpedance AFE (the MAX30009 or similar), four electrodes in the tetrapolar layout
across the neck and chest, and their leads. That is a chest-worn or patch product, not the pod. It would also need its
own validation against a reference. Thoracic bioimpedance devices tend to show small mean bias but wide individual
error against echocardiography. A 2019 meta-analysis of electrical cardiometry, a related method, set a pooled percentage error
under 30 % as acceptable ([J Clin Monit Comput 2019](https://link.springer.com/10.1007/s10877-019-00330-y)); studies of
these devices often report errors above that [VERIFY the pooled figure in the paper].

## Could a PPG-only or machine-learning estimate work at the temporal region?

**Not credibly, as an absolute stroke volume.**
- **The signal is scalp.** The pod's photon model puts the green pulse at more than 99.9 % scalp, and red and IR at
  88–89 % scalp and 11–12 % temporalis muscle (`brain-blood-flow/report.md`, claim 1). Its amplitude follows scalp
  vasomotor tone, temperature and contact pressure, not just the heart's output.
- **Pulse-contour methods track change, not level.** Finger studies estimate stroke volume from the arterial pressure
  waveform, not from optical PPG alone. Even those need an independent calibration, and under altered physiology they
  track relative change at best. For example, three finger pulse-contour algorithms were tested against
  echocardiography in 10 people at high g
  ([Manen et al. 2015, PLOS ONE](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4370450/)).
- **Machine learning needs paired truth.** There is no paired data from the temporal region with an echocardiography,
  ICG or arterial-line reference. A model trained elsewhere (finger, intensive-care data) would be validated on none of
  the pod's tissue. The depth study showed that learned models cannot pull out what the light does not carry
  (`brain-blood-flow/report.md`, claim 6). A stroke-volume model on scalp PPG would mostly learn heart rate, pulse
  amplitude and the training population's averages.

## The one head-worn route with published evidence: ballistocardiography

Each heartbeat moves the head slightly, and an accelerometer at the ear picks it up. One MIT group built an ear-worn
monitor with an accelerometer, ECG and PPG. In a proof-of-concept study of 13 people, the BCG J-wave amplitude followed
stroke volume and the R–J interval followed the pre-ejection period. The authors note the signal is weak, so readings
must be taken at rest ([He et al., IEEE TBME 2015](https://pmc.ncbi.nlm.nih.gov/articles/PMC5462526);
[EMBC 2012](https://pmc.ncbi.nlm.nih.gov/articles/PMC4384813/);
[PMC4395519](https://pmc.ncbi.nlm.nih.gov/articles/PMC4395519)).

For Atrium this is a research question, not a feature:
- the everyday modes sample the accelerometer at 12.5 Hz in low-power mode, far too slow for a BCG waveform;
- research modes 6 to 9 run it at 104 Hz (`ble_gatt.md` 4.3), and the vibration modes at 3.3 and 6.7 kHz. Whether
  the J wave clears the LSM6DSOX's noise at the temporal region is unknown [VERIFY on the bench];
- the pod has no ECG, so the R–J timing would have to use the PPG pulse foot as its anchor [ASSUMED; untested];
- the published result is one group and 13 people, ear-mounted, at rest, as a relative trend with a reference.

A useful first test, bench only: record research mode 6 at rest next to a chest ECG, and see whether a beat-locked
average of the head-to-foot axis shows a repeatable J wave. That would say whether a relative, at-rest trend is even
possible. It would still not be a stroke-volume measurement.

## Rules this note follows

- Wellness only: no stroke-volume, cardiac-output or haemodynamic claim for Atrium.
- Simulated, measured and published numbers are labelled as such. No Atrium head recording exists yet.
