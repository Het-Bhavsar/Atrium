"""S5c. Behavioural PFM buck, 1.8 V from VBAT with L1 2.2 uH and C4 derated, under AFE and radio load bursts.

Replaces s5_spice/buck_ripple.cir: ngspice was still inside that deck after 2 h 50 min at a 10 ns step, so nothing came out of it.
Fixed on-time from TPS62840 SLVSEC6D Eq. 2 (TON = Vout/Vin * 556 ns), comparator at Vref, synchronous rectifier that stops at zero current.
This checks ripple scale and droop only. It is not a stability proof. Rdson and comparator delay are [ASSUMED].
Usage: python3 -I s5c_buck_stepper.py <out.json>
"""
import json, sys
import numpy as np
from numba import njit


@njit(cache=False)
def run(vin, l, c, esr, vref, rds, td, t_end, dt):
    n = int(t_end / dt)
    il = 0.0; vc = vref
    vmin = 9.0; vmax = 0.0; ipk = 0.0; pulses = 0; pulses_idle = 0
    ton = vref / vin * 556e-9
    state = 0; t_on_left = 0.0; t_delay = -1.0
    # peak-to-peak inside a quiet stretch and inside the radio burst
    q_min = 9.0; q_max = 0.0; r_min = 9.0; r_max = 0.0
    for i in range(n):
        t = i * dt
        iload = 150e-6
        ta = (t - 1e-3) % 5e-3 if t > 1e-3 else 1.0
        if ta < 400e-6:
            iload += 1e-3
        tr = (t - 2e-3) % 10e-3 if t > 2e-3 else 1.0
        in_rf = tr < 1.5e-3
        if in_rf:
            iload += 8e-3
        vo = vc + esr * (il - iload)
        if state == 0:                          # idle, comparator watches the output, turn-on after a delay
            if vo < vref and t_delay < 0:
                t_delay = td
            if t_delay >= 0:
                t_delay -= dt
                if t_delay < 0:
                    state = 1; t_on_left = ton; pulses += 1
                    if not in_rf and ta > 400e-6:
                        pulses_idle += 1
        if state == 1:
            v_l = vin - vo - il * rds
            t_on_left -= dt
            if t_on_left <= 0:
                state = 2
        elif state == 2:
            v_l = -vo - il * rds
        else:
            v_l = 0.0
        il += v_l / l * dt
        if state == 2 and il <= 0:
            il = 0.0; state = 0
        vc += (il - iload) / c * dt
        vo = vc + esr * (il - iload)
        if t > 1e-3:
            vmin = min(vmin, vo); vmax = max(vmax, vo); ipk = max(ipk, il)
            if in_rf:
                r_min = min(r_min, vo); r_max = max(r_max, vo)
            elif ta > 500e-6 and tr > 2e-3:
                q_min = min(q_min, vo); q_max = max(q_max, vo)
    return vmin, vmax, ipk, pulses, pulses_idle, q_max - q_min, r_max - r_min


R = {}
print("Buck stepper: L 2.2 uH, load idle 150 uA, AFE 1 mA bursts, radio 8 mA bursts. 1.5 ms radio every 10 ms")
for vin in (3.0, 3.7, 4.2):
    for c in (3e-6, 6e-6, 10e-6):
        vmin, vmax, ipk, n, ni, q, r = run(vin, 2.2e-6, c, 6e-3, 1.8, 0.5, 40e-9, 12e-3, 0.25e-9)
        print(f"  Vin {vin:.1f} V, C {c*1e6:4.0f} uF: Vout {vmin:.4f} to {vmax:.4f} V, inductor peak {ipk*1e3:4.0f} mA, ripple quiet {q*1e3:5.1f} mV, in radio {r*1e3:5.1f} mV, {n} pulses in 12 ms")
        R[f"{vin}|{c}"] = dict(vmin=vmin, vmax=vmax, ipk=ipk, pulses=n, q=q, r=r)
print("  Rails: MAX86174A VDD 1.7 to 2.0 V, so the window is -100 mV and +200 mV around 1.8 V")
json.dump(R, open(sys.argv[1], "w"), indent=1, default=float)
