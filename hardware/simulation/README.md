# Simulation

The models behind Atrium's numbers. Each script lists its sources and marks assumptions `[ASSUMED]`. Committed outputs are in `results/`.

Run from the repo root after `pip install -r hardware/simulation/requirements.txt`. These run in seconds unless noted:

| Script | Computes | Command |
|---|---|---|
| `s1_optics_mc.py` | Layered photon Monte Carlo at 530 / 660 / 940 nm (830 nm reference); minutes | `python3 hardware/simulation/s1_optics_mc.py 4e7 hardware/simulation/results/s1_mc.json` |
| | Low-scatter variant | `python3 hardware/simulation/s1_optics_mc.py 1e7 hardware/simulation/results/s1_mc_lowscat.json --scat=0.5 --wl=530,940` |
| `s2_signal_budget.py` | Optical signal and noise budget per channel | `python3 hardware/simulation/s2_signal_budget.py hardware/simulation/results/s1_mc.json hardware/simulation/results/s2_budget.json` |
| `s2b_sensitivity.py` | Budget sensitivity to tissue scattering | `python3 hardware/simulation/s2b_sensitivity.py hardware/simulation/results/s1_mc.json hardware/simulation/results/s1_mc_lowscat.json hardware/simulation/results/s2b.json` |
| `s4_power_energy.py` | Bottom-up pod and case energy by mode, with datasheet currents | `python3 hardware/simulation/s4_power_energy.py hardware/simulation/results/s4.json` |
| `s5c_buck_stepper.py` | Buck ripple time-stepper | `python3 hardware/simulation/s5c_buck_stepper.py hardware/simulation/results/s5c.json` |
| `spice/*.cir` | Buck ripple, LED pulse droop, case-to-pod hot-plug and lid gating | ngspice or LTspice |
| `s6_thermal.py` | Board and skin thermal model; about 5 minutes | `python3 hardware/simulation/s6_thermal.py hardware/pod/pcb/placement/pod_placement.csv hardware/simulation/results/s6_thermal.json` |
| `s6b_thermal_fine.py` | Fine-mesh self-heating at the temperature sensor | `python3 hardware/simulation/s6b_thermal_fine.py hardware/pod/pcb/placement/pod_placement.csv hardware/simulation/results/s6b.json` |
| `s6c_extras.py` | Thermal extras from the `s6` sweep | `python3 hardware/simulation/s6c_extras.py hardware/pod/pcb/placement/pod_placement.csv hardware/simulation/results/s6c.json` |
| `s7_rf_fdtd.py` | 2.4 GHz antenna efficiency on the body, flat and raised; minutes | `python3 hardware/simulation/s7_rf_fdtd.py all hardware/simulation/results/s7_rf.json` |
| | Dipole reference | `python3 hardware/simulation/s7_rf_fdtd.py dipole hardware/simulation/results/s7_dipole.json` |
| `s9_emi_rf_calc.py` | Radio link budget and EMI estimates | `python3 hardware/simulation/s9_emi_rf_calc.py hardware/simulation/results/s7_rf.json hardware/simulation/results/s9.json` |

**Order of dependencies:**
- `s2` and `s2b` need `s1`.
- `s6b` and `s6c` import `s6`; `s6c` also reads `results/s6_thermal.json`.
- `s9` reads the `s7` output.
