# 32 MHz crystal load

Ideal C1 = C2 = 2 x (CL - C_stray) = 12.0 pF. Nordic's reference uses 12 pF: matches.

| Cx (pF) | effective CL (pF) | pulling vs 8 pF (ppm) |
|---:|---:|---:|
| 10 | 7.00 | +20.8 |
| 12 | 8.00 | +0.0 |
| 15 | 9.50 | -23.8 |

- 12 pF +/-2 % NP0 caps add +/-2.22 ppm.
- Worst case: 10 (tol) + 20 (temp) + 2.2 (caps) + 3 (aging) = 35.2 ppm, inside BLE's +/-50 ppm.
- Crystal: C0 0.54 pF +/-30 %, ESR 60 ohm max (NDK spec EXS00A-CS14160), inside Nordic's 100 ohm limit for C0 <= 3 pF.
- Layout: crystal within 2 mm of XC1/XC2, no vias in the XC traces, ground ring, nothing switching under it.