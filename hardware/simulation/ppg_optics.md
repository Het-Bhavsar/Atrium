# PPG optics at the temporal region (MAXM86161, diffusion model)

Separation 2.0 mm, PD 1.0 mm2, cover 0.9/pass, pulse 117.3 us, ADC 19 bit, AFE noise 50 pA. Dermis blood 3% (range 1%-5%), pulsatile 3% of it per beat (range 1%-6%). Values marked ASSUMED / VERIFY in the source.

Operating point = AGC: least LED current meeting 40 dB (green) / 45 dB (red, IR) per sample.

| Skin | LED | coupling (PD W / LED W) | PI (%) | ADC FS (uA) | LED (mA) | DC (uA) | AC SNR / sample (dB) | LED avg @25 sps (uA) |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Fitzpatrick I-II | green 530 | 1.59e-03 | 2.92 | 4 | 4.4 | 0.18 | 40.2 | 13 |
| Fitzpatrick I-II | red 660 | 5.62e-03 | 0.69 | 4 | 3.9 | 1.84 | 45.1 | 11 |
| Fitzpatrick I-II | IR 880 | 5.94e-03 | 0.57 | 4 | 2.9 | 2.38 | 45.1 | 9 |
| Fitzpatrick III-IV | green 530 | 9.77e-04 | 2.92 | 4 | 7.1 | 0.18 | 40.1 | 21 |
| Fitzpatrick III-IV | red 660 | 4.37e-03 | 0.69 | 4 | 5.0 | 1.83 | 45.0 | 15 |
| Fitzpatrick III-IV | IR 880 | 5.34e-03 | 0.57 | 4 | 3.3 | 2.41 | 45.2 | 10 |
| Fitzpatrick V-VI | green 530 | 2.42e-04 | 2.92 | 4 | 28.2 | 0.18 | 40.0 | 83 |
| Fitzpatrick V-VI | red 660 | 2.12e-03 | 0.69 | 4 | 10.2 | 1.82 | 45.0 | 30 |
| Fitzpatrick V-VI | IR 880 | 3.93e-03 | 0.57 | 4 | 4.4 | 2.37 | 45.1 | 13 |

Monte Carlo over blood fraction, pulsatility and separation (+/-0.1 mm), AC SNR per sample (dB), 5th / 50th percentile:

| Skin | green 530 | red 660 | IR 880 |
|---|---:|---:|---:|
| Fitzpatrick I-II | 40.0 / 40.1 | 45.0 / 45.1 | 45.0 / 45.1 |
| Fitzpatrick III-IV | 40.0 / 40.1 | 45.0 / 45.1 | 45.0 / 45.1 |
| Fitzpatrick V-VI | 40.0 / 40.0 | 45.0 / 45.0 | 45.0 / 45.1 |

LED current the AGC needs (mA), 50th / 95th percentile, and green average current at 25 sps (uA, 95th):

| Skin | green | red | IR | green avg @25 sps |
|---|---:|---:|---:|---:|
| Fitzpatrick I-II | 4.0 / 11.7 | 3.6 / 18.1 | 2.7 / 15.6 | 34 |
| Fitzpatrick III-IV | 6.4 / 19.2 | 4.7 / 24.9 | 3.2 / 16.2 | 56 |
| Fitzpatrick V-VI | 25.5 / 73.9 | 9.6 / 48.1 | 4.3 / 22.1 | 217 |

## Reading it

- Heart rate needs ~20 dB in the HR band. 25 sps averaging adds 6.2 dB, so the worst 5th-percentile green channel reaches 46.2 dB in band: OK before motion.
- Relative SpO2 trend (red / IR ratio) wants ~30 dB per channel; the worst 5th percentile is 45.0 dB per sample (51.2 dB in band): OK. This stays a trend, never a medical SpO2 claim.
- Dark skin costs green the most (melanin absorbs ~3x more at 530 than at 880 nm); the firmware's AGC raises LED current, which the table shows as higher LED mA and average current. Where the LED hits 124 mA the ADC range drops instead.
- Battery: compare 'LED avg' with the power budget's green term; dark-skin wearers pay more LED current at the same SNR.
- The model ignores motion artefact, which dominates at the temporal region during chewing and talking: the IMU-gated sampling windows in firmware matter more than optics.
- Bench plan (EVT): flat phantom (Intralipid + ink) at 3 absorptions, then 10 people across skin types; fit coupling and PI, then update the ASSUMED constants here.
