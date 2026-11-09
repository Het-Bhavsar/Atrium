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

## MAXM86161A (fitted from 2026-11-10)

The pod now fits the MAXM86161A (data sheet 19-102034 Rev 1). The package, spacing, photodiode, ADC and register map
are the MAXM86161's. Only the LEDs differ, so the model above is re-run with the green LED's output scaled and nothing
else changed. Two scalings bracket it:
- **×1.70, detected light (used for decisions).** The data sheets' production-test counts at 124 mA: green 115000 at
  the 32 µA range against 135000 at 16 µA.
- **×2.40, radiant power (upper bound).** 70 mW at 175 mA against 25 mW at 150 mA, per mA.

Red and IR read the same counts on both parts (200000 and 155000 at 32 µA), so they are unchanged. Continuous LED
current (not 8-bit steps) and a fresh draw of 4000 cases, so the MAXM86161 column differs slightly from the tables
above.

Share of cases meeting the per-sample SNR target at the firmware's fixed green currents (`firmware/src/power/modes.c`),
MAXM86161 / MAXM86161A ×1.70 / ×2.40:

| Skin | green 15 mA (full, smart) | green 10 mA (sleep, lite) | green 5 mA (probe) |
|---|---:|---:|---:|
| Fitzpatrick I-II | 99.0 / 100 / 100 % | 90.8 / 99.7 / 100 % | 63.5 / 86.4 / 95.6 % |
| Fitzpatrick III-IV | 88.4 / 99.3 / 100 % | 73.8 / 91.7 / 98.8 % | 31.7 / 65.8 / 81.7 % |
| Fitzpatrick V-VI | 9.4 / 48.8 / 68.1 % | 0.1 / 18.4 / 44.8 % | 0 / 0 / 1.2 % |

Green current that meets the target (mA, 50th / 95th percentile), and the highest green DC at 15 mA:

| | Fitzpatrick I-II | Fitzpatrick III-IV | Fitzpatrick V-VI | green DC at 15 mA, max |
|---|---:|---:|---:|---:|
| MAXM86161 | 4.0 / 11.7 | 6.5 / 19.0 | 26.2 / 76.6 | 12.5 % of 16 µA |
| MAXM86161A ×1.70 | 2.3 / 6.9 | 3.8 / 11.2 | 15.4 / 45.0 | 21.2 % |
| MAXM86161A ×2.40 | 1.7 / 4.9 | 2.7 / 7.9 | 10.9 / 31.9 | 30.0 % |

### What the brighter green allows

- **More signal on darker skin at the same current.** The firmware keeps green at 15 / 10 / 5 mA. At 15 mA, darker
  skin (V-VI) meets the target in 49 % of cases instead of 9 %, and III-IV in 99 % instead of 88 %. This is the main
  gain, and it needs no firmware change. Dark skin is still the weak case: at 15 mA half of V-VI falls short.
- **No saturation from green.** Green DC stays at 21–30 % of the 16 µA range at 15 mA. The range stays at 16 µA.
  Red and IR are unchanged. IR at 10 mA on light skin is still the channel closest to full scale (79 % in the model).
  The model's IR LED efficiency (0.25 W/A) is below the data sheet's 38 mW at 100 mA. If the real figure is nearer the
  data sheet, IR on light skin reaches full scale. That predates the part change, and the bench checks it (O-43).
- **An ADC range step is not worth adding now.** In the model, quantization at 16 µA (9 pA RMS) is small against the
  AFE (50 pA) and shot noise, so a lower range gains under 0.1 dB. It would matter only if the ADC's noise is fixed
  in counts. The data sheet gives that floor only at the 4 µA range (5–25 counts), so the bench measures it per range
  first (O-43).
- **Lower forward voltage.** Green is 3.4 V max at 175 mA (was 3.8 V), so `green-min-mv` is 3.1 V and the green
  limit sits under the 3.3 V cut-off. Green runs to the end of the charge, and the IR fallback stays only as a guard.

### Battery: a green trim, sized but not built

Lowering every mode to the MAXM86161's light (9 / 6 / 3 mA) would save 22.5 µA in full and smart, 15.0 µA in sleep,
3.7 µA in lite and 2.4 µA in the probe. That is +5 % (full), +12 % (smart) and +13 % (sleep) of battery, for everyone.
It would also give back the darker-skin gain above, so the table does not do it.

A trim would lower green only where there is margin to spare. It would step down only, never below the mode's current
/ 1.7 (so never less light than the MAXM86161 gave). It would aim the green DC at 18000 counts, the DC at which the
5th-percentile perfusion still meets the target (0.56 µA). With ×1.70:

| Skin | median green, 15 mA mode | LED current, 15 mA mode | cases meeting the target | 10 mA mode (sleep) |
|---|---:|---:|---:|---:|
| Fitzpatrick I-II | 9 mA | 38.1 µA (was 56.2) | 99.8 % (100) | 30.0 µA (was 37.4) |
| Fitzpatrick III-IV | 13 mA | 46.0 µA | 96.4 % (99.3) | 35.0 µA |
| Fitzpatrick V-VI | 15 mA (no trim) | 56.2 µA | 48.8 % (48.8) | 37.4 µA |

That is +9 % (light) and +5 % (medium) battery in smart, +4 % and +2 % in full, +6 % and +2 % in sleep, and nothing
for darker skin. It is not built, for four reasons:
- the ML data contract fixes the LED at the mode's current (the stream and fit rules in `docs/ble_gatt.md` 4.2 assume no LED AGC);
- its target, the DC-versus-current linearity and the noise floor are bench numbers;
- a mid-wear change restarts the module, which costs the pulse lock and part of an HRV window;
- the gain is small.

If the bench supports it, the design is ready:
- learn the target from wear's on-skin DC per mA (`wear.c`, its first minute with a pulse);
- hold it for the wear;
- run at the table current whenever the raw stream, the fit guide, a research mode or an oxygen burst runs, so the
  stream contract does not change;
- go back to the table current if DC per mA falls under 0.7 of the reference.
