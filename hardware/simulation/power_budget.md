# Power budget (locked 20.9 mm build)

Average current per load in uA. Estimates until measured with a Nordic PPK2.

| Load | low | typ | high |
|---|---:|---:|---:|
| nRF52832 BLE link, 1 s interval | 15.0 | 22.0 | 30.0 |
| nRF52832 DSP (HR FFT 1 Hz, HRV, logging) | 20.0 | 35.0 | 60.0 |
| nRF RC LFCLK + calibration | 1.0 | 2.0 | 4.0 |
| MAXM86161 AFE @25 sps | 5.0 | 8.0 | 10.0 |
| Green LED @25 sps, PA 15-35 % | 22.5 | 37.5 | 52.5 |
| LSM6DSOX accel LP + MLC | 5.0 | 12.0 | 26.0 |
| BQ21061 LP mode + VDD LDO | 0.5 | 0.9 | 1.9 |
| AP6683 protection | 0.1 | 0.4 | 0.7 |
| TPS7A0218 Iq | 0.0 | 0.0 | 0.1 |
| VSYS divider 2.2M/1M | 1.0 | 1.2 | 1.4 |
| NTC dividers, 1 ms @1 Hz x2 | 0.2 | 0.2 | 0.3 |
| I2C pull-ups (bus activity) | 1.0 | 2.0 | 4.0 |
| HRV window: green @100 sps, 60 s every 5 min (avg, duty 20%) | 13.5 | 22.5 | 31.5 |
| SpO2 burst: red+IR @100 sps, 30 s every 10 min (avg, duty 5%) | 12.0 | 18.0 | 24.0 |
| Gyro on (motion rejection), 5 % of time (avg, duty 5%) | 27.5 | 27.5 | 32.5 |
| **Total, with bursts** | **124** | **189** | **279** |
| Total, continuous loads only | 71 | 121 | 191 |

## Days of wear on one charge

| Cell state | at high | at typ | at low |
|---|---:|---:|---:|
| New (80% of 30 mAh) | 3.6 | 5.3 | 8.0 |
| Aged (70%) | 3.1 | 4.6 | 7.0 |

## Findings

- HRV at 100 sps *continuously* would add ~112 uA typ (LED alone), cutting life to ~4.3 days. Use 100 sps windows, or 25 sps with interpolation (see hrv_timing).
- The LED and the MCU's DSP are the two biggest levers. BLE interval matters less than people expect at 1 s.
- Gyro: every 1 % of always-on time costs ~5.5 uA.
- VLED headroom: green LED forward voltage plus driver compliance (253 mV max at the 31 mA range) is the real limit. The MAXM86161A's green is 3.4 V max at 175 mA (the MAXM86161 was 3.8 V), so firmware's green limit is 3.1 V plus the pulse sag, under the 3.3 V cut-off: green runs to the end of the charge (`green-min-mv` in `atrium_pod.dts`; measure with O-16).
- MAXM86161A LED current: the green terms here stay at the firmware's 15 / 10 mA. The brighter green (about 1.7× the detected light) goes to signal margin on darker skin, not to battery. A green trim would save 2–9 % for lighter skin (`ppg_optics.md`, MAXM86161A section); it waits for the bench (O-43).
