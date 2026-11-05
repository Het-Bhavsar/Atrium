"""S9. Hand-check physics that does not need a field solver: buck-to-photodiode coupling, CPWG geometry, BLE link budget.

Buck pulse shape from TPS62840 SLVSEC6D Eq. 2: TON = Vout/Vin * 556 ns. PD capacitance from VEMD5080X01: 80 pF at 0 V, 30 pF at 3 V.
Loop areas are [ASSUMED]. The answer is an order of magnitude to decide whether the 5 mm rule can be waived with a solid L2.
Usage: python3 -I s9_emi_rf_calc.py <s7.json or ''> <out.json>
"""
import json, math, sys
import numpy as np
from scipy import special, optimize

R = {}
print("Buck to PD input, magnetic coupling")
VIN, VO, L = 3.7, 1.8, 2.2e-6
ton = VO / VIN * 556e-9
ipk = (VIN - VO) / L * ton
toff = L * ipk / VO
q = 0.5 * ipk * (ton + toff)
print(f"  TON {ton*1e9:.0f} ns, inductor peak {ipk*1e3:.0f} mA, pulse {1e9*(ton+toff):.0f} ns, {q*1e9:.0f} nC per pulse; at 4 mA load {4e-3/q/1e3:.0f} kHz, so {4e-3/q*117e-6:.1f} pulses per 117 us exposure")
didt = (VIN - VO) / L
mu0 = 4e-7 * math.pi
A_hot, A_pd = 4e-6, 2e-6
C_pd = 65e-12         # MAX86174A noise condition and VEMD 0 V to 3 V range bracket this
T = 117.1e-6
R["buck"] = dict(ton=ton, ipk=ipk, pulse=ton + toff, q=q)
rows = {}
for r_mm in (1.5, 2, 3, 4, 5, 7):
    r = r_mm * 1e-3
    M = mu0 * A_hot * A_pd / (4 * math.pi * r ** 3)
    v = M * didt
    # The input is a virtual ground, so a step of induced EMF moves C*dV of charge. Up and down ramps nearly cancel per pulse;
    # the residual that matters is one uncancelled edge at the exposure boundary.
    qe = C_pd * v
    i_eq = qe / T
    for_label = 0.3     # shield factor: L2 copper 35 um against 50 um skin depth at the pulse frequency, wide plane [ASSUMED]
    rows[r_mm] = dict(M_pH=M * 1e12, v_uV=v * 1e6, q_fC=qe * 1e15, i_pA=i_eq * 1e12, i_shield_pA=i_eq * for_label * 1e12)
    print(f"  r {r_mm:>3} mm: M {M*1e12:7.1f} pH, EMF {v*1e6:7.1f} uV, charge {qe*1e15:7.2f} fC, equivalent {i_eq*1e12:7.1f} pA; with L2 about {i_eq*for_label*1e12:6.1f} pA  (AFE noise 75 pA)")
R["buck"]["rows"] = rows
print("  Capacitive: SW edge 3.7 V. 1 fF of stray to PD_K puts 3.7 fC on the input, about 32 pA equivalent. L2 between them makes this negligible; do not route SW and PD_K on one layer.")


def cpwg_z(w, g, h, er):
    """CPWG with a lower ground plane, Wadell. Copper thickness ignored; solder mask lowers Z by 1-3 ohm."""
    a = w / 2; b = w / 2 + g
    k = a / b; kp = math.sqrt(1 - k ** 2)
    k1 = math.tanh(math.pi * a / (2 * h)) / math.tanh(math.pi * b / (2 * h)); k1p = math.sqrt(1 - k1 ** 2)
    K = lambda m: special.ellipk(m ** 2)
    x = K(k) / K(kp) + K(k1) / K(k1p)
    ee = (1 + er * (K(k1) / K(k1p)) / (K(k) / K(kp))) / (1 + (K(k1) / K(k1p)) / (K(k) / K(kp)))
    return 60 * math.pi / math.sqrt(ee) / x, ee


print("\n50 ohm CPWG on L1 over L2 for JLC prepreg options. Confirm with the JLC calculator for the stackup actually quoted.")
R["cpwg"] = {}
for name, h, er in (("3313 0.0994 mm, er 4.1", 0.0994, 4.1), ("2116 0.1164 mm, er 4.16", 0.1164, 4.16), ("1080 0.0764 mm, er 3.91", 0.0764, 3.91)):
    for g in (0.10, 0.127, 0.15, 0.20):
        f = lambda w: cpwg_z(w, g, h, er)[0] - 50.0
        w = optimize.brentq(f, 0.03, 0.6)
        ee = cpwg_z(w, g, h, er)[1]
        print(f"  {name:<25} gap {g:.3f} mm: width {w:.3f} mm, eps_eff {ee:.2f}")
        R["cpwg"][f"{name} g{g}"] = dict(w=w, ee=ee)

print("\nBLE link budget at 2.44 GHz, 1 Mbps, 0 dBm, nRF52832 sensitivity -96 dBm")
R["link"] = {}
s7 = {}
if len(sys.argv) > 1 and sys.argv[1]:
    try:
        s7 = json.load(open(sys.argv[1]))
    except Exception:
        s7 = {}
base = {"datasheet-style free space": 2.0}
for k, v in s7.items():
    if isinstance(v, dict) and "eta" in v and 0 < v["eta"] < 1:    # the floating-cell free-space run did not settle, skip it
        base[v["name"]] = 10 * math.log10(max(v["eta"], 1e-3)) + 1.5
for k, gain in base.items():
    for ptx in (0, 4):
        for label, extra, rx_sens, rx_gain in (("phone in hand", 6, -90, -3), ("phone in far pocket", 25, -90, -3), ("phone on desk, 5 m office", 12, -90, -3)):
            margin_budget = ptx + gain + rx_gain - extra - rx_sens
            d = 10 ** ((margin_budget - 40.2) / 25)    # path exponent 2.5 indoors, 40.2 dB at 1 m
            R["link"][f"{k}|{ptx}|{label}"] = dict(margin_at_1m=margin_budget - 40.2, range_m=d)
        if ptx == 0:
            print(f"  {k:<46} gain {gain:5.1f} dBi: range hand {10**((0+gain-3-6+90-40.2)/25):5.1f} m, far pocket {10**((0+gain-3-25+90-40.2)/25):5.1f} m, desk {10**((0+gain-3-12+90-40.2)/25):5.1f} m")

print("\nI2C and AFE FIFO")
for cbus in (30e-12, 50e-12, 80e-12):
    tr = 0.8473 * 4.7e3 * cbus
    print(f"  4.7k pull-ups, {cbus*1e12:.0f} pF bus: rise {tr*1e9:.0f} ns, Fast-mode limit 300 ns")
    R[f"i2c_tr_{int(cbus*1e12)}"] = tr
print("  Addresses on the one bus: MAX86174A ADDR=GND gives 0x6A, LSM6DSV16X SA0=GND gives 0x6A (conflict), TMP117 0x48, MAX17048 0x36")
for name, words_s in (("full, 3 LEDs x 2 PDs at 50 Hz", 300), ("smart", 150), ("lite", 50), ("sleep", 74)):
    print(f"  FIFO 256 words, {name}: full in {256/words_s:.2f} s, so read at least every {0.8*256/words_s:.2f} s")
    R[f"fifo_{name}"] = 256 / words_s
json.dump(R, open(sys.argv[2], "w"), indent=1, default=float)
