# NTC + SAADC (TH1 skin, TH4 top)

Topology: VDD (NTC_EN) -> 10 k 0.1 % -> AIN -> NCU15XH103F60RC -> GND, ratiometric (gain 1/4, ref VDD/4).

| Temp (degC) | node V | code | codes/degC | degC per LSB |
|---:|---:|---:|---:|---:|
| 20 | 0.987 | 2245 | 39.9 | 0.025 |
| 25 | 0.900 | 2048 | 38.9 | 0.026 |
| 30 | 0.816 | 1857 | 37.3 | 0.027 |
| 34 | 0.752 | 1711 | 35.7 | 0.028 |
| 37 | 0.706 | 1606 | 34.3 | 0.029 |
| 40 | 0.661 | 1505 | 32.8 | 0.030 |
| 45 | 0.592 | 1348 | 30.2 | 0.033 |

## Error budget (Monte Carlo, 20k units, beta model)

NTC R25 +/-1 %, B +/-1 %, fixed R +/-0.1 % (all 3 sigma), SAADC 2 LSB rms (ASSUMPTION).

| Temp | mean error | 1 sigma | 99 % bound |
|---:|---:|---:|---:|
| 25 degC | +0.001 | 0.102 | 0.262 degC |
| 34 degC | +0.001 | 0.113 | 0.295 degC |
| 37 degC | +0.000 | 0.119 | 0.309 degC |
| 42 degC | -0.000 | 0.131 | 0.336 degC |

## Self-heating and settling

- While NTC_EN is high: 105 uA, 79 uW in the NTC -> 79 mdegC rise if left on.
- At 1 ms per second the average rise is ~78.8 udegC: negligible. Keep NTC_EN pulsed.
- Source impedance at 34 degC: 4.18 kohm -> SAADC TACQ = 3 us is enough (nRF52832 allows up to 10 kohm at 3 us).
- With a 10 nF filter cap at the AIN pin: tau = 42 us; wait >= 376 us after NTC_EN rises for 12-bit settling.
- Without the cap, 50 us after NTC_EN is plenty. Recommendation: fit a 10 nF 0201 footprint, populate only if EVT noise needs it.

## Firmware change

Old sketch: NTC on top, 10 k to ground. New: 10 k on top, NTC to ground. In ntc.c: R_ntc = R_fixed * code / (4096 - code).
The table in `ntc_lut.h` is generated from the beta model; regenerate from Murata's R-T table before shipping.