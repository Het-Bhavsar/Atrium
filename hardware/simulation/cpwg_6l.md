# 50 ohm feed on the 6-layer stack (L1 over L2, prepreg 2313)

| case | w | gap | h | er | Z0 (ohm) | eeff |
|---|---|---|---|---|---|---|
| drawn CPWG, mask | 0.124 | 0.127 | 0.0964 | 4.10 | 52.7 | 3.05 |
| same track, no coplanar GND (microstrip) | 0.124 | 0.800 | 0.0964 | 4.10 | 56.1 | 3.14 |
| tol w-0.012 h-10% er-0.15 | 0.112 | 0.139 | 0.0868 | 3.95 | 53.7 | 3.02 |
| tol w-0.012 h-10% er+0.15 | 0.112 | 0.139 | 0.0868 | 4.25 | 52.3 | 3.18 |
| tol w-0.012 h+10% er-0.15 | 0.112 | 0.139 | 0.1060 | 3.95 | 60.4 | 2.76 |
| tol w-0.012 h+10% er+0.15 | 0.112 | 0.139 | 0.1060 | 4.25 | 59.1 | 2.89 |
| tol w+0.012 h-10% er-0.15 | 0.136 | 0.115 | 0.0868 | 3.95 | 48.0 | 3.01 |
| tol w+0.012 h-10% er+0.15 | 0.136 | 0.115 | 0.0868 | 4.25 | 46.8 | 3.18 |
| tol w+0.012 h+10% er-0.15 | 0.136 | 0.115 | 0.1060 | 3.95 | 53.8 | 2.77 |
| tol w+0.012 h+10% er+0.15 | 0.136 | 0.115 | 0.1060 | 4.25 | 52.6 | 2.9 |

Tolerance corners: 46.8 - 60.4 ohm (JLC impedance control is +/-10%: 45-55 ohm).
Most of the chain is 0.2-0.9 mm runs between 0201 pads (under 2 % of a guided wavelength at 2.44 GHz): the
pi match (C42 / Z41 / C40) is tuned on the bench, so the microstrip-vs-CPWG difference shows up as a small series
L the match absorbs.
