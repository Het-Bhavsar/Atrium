# HRV (RMSSD) timing error vs PPG sample rate

Synthetic RR series, true RMSSD = 33.5 ms, SNR 30 dB, 20 trials each.

| sps | no interp: bias ms | sd ms | parabolic interp: bias ms | sd ms |
|---:|---:|---:|---:|---:|
| 25 | +12.7 | 1.3 | +2.1 | 0.4 |
| 50 | +4.0 | 1.1 | +1.1 | 0.5 |
| 100 | +1.3 | 0.3 | +0.7 | 0.4 |
| 200 | +0.4 | 0.3 | +0.3 | 0.2 |

## Reading it

- The detector low-passes at 8 Hz (3rd-order Butterworth, zero phase) before differentiating, as src/hrv.c should.
- Quantising beat times to the sample grid adds noise to every RR interval, which RMSSD (a difference measure) amplifies into a positive bias.
- Parabolic interpolation of the derivative peak removes most of it at 25 sps, which is far cheaper in LED power than running at 100 sps (see power_budget).
- Synthetic data is kinder than skin at the temporal region (motion, low perfusion). Treat these as a floor and validate against a chest-strap ECG in EVT.