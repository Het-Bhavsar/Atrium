# Simulation

These are the models behind Atrium's numbers. Every script lists its sources and marks assumptions `[ASSUMED]`. Results are in `results/`.

| Script | What it computes |
|---|---|
| `s2_signal_budget.py`, `s2b_sensitivity.py` | Optical signal and noise budget per channel; reads `results/s1_mc.json`, the photon Monte Carlo output |
| `green_budget.py` | Green-channel photocurrent and SNR by LED current |
| `s4_power_energy.py` | Bottom-up pod and case energy model by mode, with datasheet currents |
| `s5c_buck_stepper.py`, `spice/*.cir` | Buck ripple, LED pulse droop and case-to-pod hot-plug (ngspice / LTspice) |
| `s6_thermal.py`, `s6b_thermal_fine.py`, `s6c_extras.py` | Board and skin thermal model; self-heating at the temperature sensor |
| `s7_rf_fdtd.py` | 2.4 GHz antenna efficiency on the body, flat and raised 2.5 mm |
| `s9_emi_rf_calc.py` | Radio link budget and EMI estimates |

Run each one with `python3 <script>.py <args>`; the usage line is at the top of each file.
