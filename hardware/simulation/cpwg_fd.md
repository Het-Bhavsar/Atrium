# 50 ohm CPWG on the radio island: 2-D field solver

Finite-difference Laplace solve of the real cross-section (trapezoid 35 um copper, mask, 3313 prepreg over solid L2). Grid 2.5 um. Mask er 3.8, 15 um over copper, 25 um over laminate, etch 10 um per side: ASSUMPTIONS [VERIFY with JLC].

## Drawn line (0.128 mm, 0.127 mm gap)

| Case | Z0 (ohm) | eeff | lambda_g at 2.44 GHz (mm) |
|---|---:|---:|---:|
| with mask | 52.3 | 3.05 | 70.3 |
| no mask | 56.3 | 2.64 | 75.6 |

## Width sweep (gap 0.127, with mask)

| w (mm) | Z0 (ohm) |
|---:|---:|
| 0.100 | 58.3 |
| 0.110 | 56.1 |
| 0.120 | 54.1 |
| 0.128 | 52.3 |
| 0.140 | 50.5 |
| 0.150 | 48.9 |
| 0.160 | 47.4 |
| 0.180 | 44.7 |
| 0.200 | 42.3 |

## Fab tolerance (with mask, w 0.128 / g 0.127 nominal)

| Case | Z0 (ohm) |
|---|---:|
| w -0.015 (over-etch) | 56.1 |
| w +0.015 (under-etch) | 48.8 |
| prepreg -10 % (0.0895) | 50.1 |
| prepreg +10 % (0.1093) | 54.3 |
| er 3.9 | 53.3 |
| er 4.3 | 51.4 |

## Verdict

- The solver puts 50.0 ohm at w = 0.143 mm with the assumed mask; the drawn 0.128 mm gives 52.3 ohm.
- JLC builds to its own calculator (0.128 mm). Order with impedance control on this net class, 50 ohm +/-10 %, coplanar single-ended with mask, L2 reference, and let JLC adjust width for their mask and etch.
- The whole line is under 5 mm (< 0.07 lambda_g), so a few ohm of error costs < 0.05 dB mismatch loss; the pi network on the VNA absorbs it.
## Solver check

Thin copper (2.5 um), no mask, no etch, w 0.196 / g 0.127: the solver gives 48.8 ohm, eeff 2.99; the closed-form model gives 50.0 ohm, 3.06 (2.4 % apart).
