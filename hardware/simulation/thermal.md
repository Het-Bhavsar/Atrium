# Pod thermal (steady state)

Shell to air 177 K/W, base plate 9.5 K/W, spreading into skin 56 K/W.

| Worn mode | P (mW) | skin rise under the pod (C) | TH1 bias (C) |
|---|---:|---:|---:|
| typical (189 uA) | 0.72 | 0.029 | 0.029 |
| worst (279 uA + 217 uA green, Fitz. V-VI) | 1.88 | 0.077 | 0.077 |
| HRV burst (green 100 sps, 60 s) | 4.56 | 0.187 | 0.187 |

Charging in the Plinth: 24.2 mW (BQ21061 linear drop at VBAT 3.5 V, 15 mA) -> pod rise 9.2 C over ambient (nest 379 K/W).

## Reading it

- Worn, the electronics warm the skin by hundredths of a degree: far below the 41 C contact limit and below the 0.1 C skin-trend budget, so TH1 needs no self-heating correction. The HRV burst is the largest term; firmware can skip the TH1 sample during it.
- Charging: ~9 C above ambient at the start of CC, falling as VBAT rises. At 35 C ambient that is close to the cell's 45 C charge limit, so the BQ21061 TS input (TH5 beside the cell) and firmware JEITA (reduced current above 40 C, stop at 45 C) are required, and the Plinth nest should be open, not a closed lid.
- Assumptions: convection and nest resistance are estimates [VERIFY with a thermocouple on the EVT pod, worn and charging].
