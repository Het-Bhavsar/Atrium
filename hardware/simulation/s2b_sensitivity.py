"""S2b. How much do the headline photocurrents move with tissue scattering and with the window geometry?
Reuses the S2 ray path. Usage: python3 s2b_sensitivity.py <s1_mc.json> <s1_mc_lowscat.json> <out.json>
"""
import json, math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s2_signal_budget as s2

mc = json.load(open(sys.argv[1]))
low = json.load(open(sys.argv[2]))
rng = np.random.default_rng(7)
D2 = (2.2, 7.0)
PD1 = (8.2, 7.0)
PD1B = (5.7, 7.0)
PD2 = (20.7, 7.0)
R = {}


def current(mcx, wl, case, chip, led_c, ap_d, pd, ap, ma, gap_led=s2.GAP_LED, gap_pd=s2.GAP_PD, bin_="min", aperture_c=None):
    saved = s2.CHIP[wl]
    s2.CHIP[wl] = chip
    sx, sy, sT, f_led = s2.led_to_skin(wl, led_c, ap_d, ap_d + 0.2, aperture_c or led_c, 200000, rng, gap_led=gap_led)
    s2.CHIP[wl] = saved
    r = s2.Rint(mcx, wl, case)
    f_pd, _, vign = s2.collect(r, sx, sy, sT, pd, ap[0], ap[1], 0.1, pd, 2000000, rng, gap_pd=gap_pd)
    phi = math.pi * s2.IE_20MA[wl][bin_] * ma / 20
    return phi * f_led * f_pd * s2.RESP[wl], f_led, f_pd, vign


G_FAR = (-0.36, 0.0); G_NEAR = (0.36, 0.0); IR_CH = (0.26, 0.26)
print("Scattering sensitivity (all tissue scattering x 0.5). Green 15 mA, IR 50 mA, min bin, medium skin, nominal blood")
print(f"{'case':<36}{'nominal':>12}{'low scatter':>14}{'ratio':>8}")
for name, wl, chip, pd, ap, ma in (("green, as drawn, slot", 530, G_FAR, PD1, (1.0, 2.5), 15),
                                   ("green, turned, open, 6 mm", 530, G_NEAR, PD1, (2.8, 2.8), 15),
                                   ("green, turned, open, 3.5 mm", 530, G_NEAR, PD1B, (2.8, 2.8), 15),
                                   ("IR near slot 50 mA", 940, IR_CH, PD1, (1.0, 2.5), 50),
                                   ("IR far 50 mA", 940, IR_CH, PD2, (2.8, 2.8), 50)):
    a = current(mc, wl, "medium|nominal", chip, D2, 1.6, pd, ap, ma)[0]
    b = current(low, wl, "medium|nominal", chip, D2, 1.6, pd, ap, ma)[0]
    print(f"{name:<36}{a*1e9:10.2f}nA{b*1e9:12.2f}nA{b/a:8.1f}")
    R[name] = dict(nominal=a, low_scatter=b)

print("\nLED aperture, green through to skin, D2 turned so green is 0.36 mm toward PD1. PD1 open at 3.5 mm")
for label, ap_d, ac, gap in (("Oe1.6 on package centre (spec)", 1.6, D2, 0.40),
                             ("Oe1.6 on the green chip", 1.6, (2.56, 7.0), 0.40),
                             ("Oe2.0 on package centre", 2.0, D2, 0.40),
                             ("Oe2.0 on package centre, LED 0.2 mm nearer window", 2.0, D2, 0.20),
                             ("Oe1.6 on the green chip, LED 0.2 mm nearer window", 1.6, (2.56, 7.0), 0.20)):
    i, fl, fp, v = current(mc, 530, "medium|nominal", G_NEAR, D2, ap_d, PD1B, (2.8, 2.8), 15, gap_led=gap, aperture_c=ac)
    i2, fl2, _, _ = current(mc, 940, "medium|nominal", (-IR_CH[0], IR_CH[1]), D2, ap_d, PD2, (2.8, 2.8), 50, gap_led=gap, aperture_c=ac)
    print(f"  {label:<52} green {i*1e9:7.1f} nA (LED path {fl:.2f})   far IR {i2*1e9:6.2f} nA (LED path {fl2:.2f})")
    R["led_" + label] = dict(green=i, f_led=fl, far=i2, f_led_ir=fl2)

print("\nPD window, green, PD1 at 3.5 mm. Aperture size and stand-off")
for label, ap, gap_pd in (("2.8 x 2.8, 0.15 mm gap (spec)", (2.8, 2.8), 0.15), ("3.4 x 3.4, 0.15 mm gap", (3.4, 3.4), 0.15), ("3.4 x 3.4, 0.05 mm gap", (3.4, 3.4), 0.05), ("2.8 x 2.8, 0.05 mm gap", (2.8, 2.8), 0.05)):
    i, fl, fp, v = current(mc, 530, "medium|nominal", G_NEAR, D2, 1.6, PD1B, ap, 15, gap_pd=gap_pd)
    print(f"  {label:<36} green {i*1e9:7.1f} nA, share of skin exit flux reaching the active area {v:.2f}")
    R["pd_" + label] = dict(green=i, vign=v)

print("\nGreen LED current needed for a pulse SNR of 20 dB after the HR band, PD1 at 3.5 mm, min bin")
for case in ("light|nominal", "medium|nominal", "dark|nominal", "very_dark|nominal", "medium|high", "very_dark|high"):
    r0 = current(mc, 530, case, G_NEAR, D2, 1.6, PD1B, (2.8, 2.8), 15)[0]
    ref = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "results/s2_budget.json")))["options"]["D turned, open, PD1 at 3.5 mm"][case]
    ac = ref["ac"] / 3
    target = 10 ** ((20 - 8.1) / 20)
    # noise is dominated by AFE below 4 uA, so SNR grows with current until shot noise matters
    ma = 15 * target * s2.noise(r0) / (r0 * ac)
    print(f"  {case:<20} {r0*1e9:8.2f} nA at 15 mA -> needs {ma:7.1f} mA   (driver 32 mA range, SFH 7016 green 30 mA DC max)")
    R["needs_" + case] = ma

print("\nIR near as a fallback, 5 mA, PD1 at 3.5 mm, min bin")
for case in ("medium|nominal", "very_dark|nominal", "very_dark|high"):
    i = current(mc, 940, case, (0.26, -0.26), D2, 1.6, PD1B, (2.8, 2.8), 5)[0]
    print(f"  {case:<20} {i*1e9:8.1f} nA")
json.dump(R, open(sys.argv[3], "w"), indent=1, default=float)
