"""Green vs IR light budget on Atrium PD1 at 6 mm, using Farrell 1992 diffusion for a semi-infinite medium.
Scaled from the package's own IR Monte Carlo photocurrents so window and geometry assumptions cancel.
Rough estimate: homogeneous skin, no vessel packaging, no optical crosstalk. Not a replacement for a layered MC at 530 nm."""
import math

import numpy as np

Q = 1.602e-19
T_INT = 117e-6      # package sim_all.py
I_AFE_N = 50e-12    # package budget, [VERIFY] against MAX86174A
BW = 1 / (2 * T_INT)
HB_MOLAR = 150 / 64500          # 150 g/L whole blood
EPS_HBO2_530 = 39956            # cm^-1/M, Prahl tabulation at 530 nm
MUA_BLOOD_530 = 2.303 * EPS_HBO2_530 * HB_MOLAR / 10    # per mm


def farrell(r, mua, musp, n=1.4):
    """Diffuse reflectance per unit area and per incident watt, mm^-2. Farrell, Patterson, Wilson 1992."""
    mut = mua + musp
    a = musp / mut
    mueff = math.sqrt(3 * mua * mut)
    D = 1 / (3 * mut)
    Rd = -1.440 / n**2 + 0.710 / n + 0.668 + 0.0636 * n
    A = (1 + Rd) / (1 - Rd)
    z0 = 1 / mut
    zb = 2 * A * D
    r1 = np.sqrt(z0**2 + r**2)
    r2 = np.sqrt((z0 + 2 * zb) ** 2 + r**2)
    return a / (4 * math.pi) * (
        z0 * (mueff + 1 / r1) * np.exp(-mueff * r1) / r1**2
        + (z0 + 2 * zb) * (mueff + 1 / r2) * np.exp(-mueff * r2) / r2**2
    )


def collected(x0, x1, y0, y1, mua, musp, src_x=0.0, N=400):
    """Fraction of incident power landing on a rectangular aperture, source at (src_x, 0)."""
    xs = np.linspace(x0, x1, N)
    ys = np.linspace(y0, y1, N)
    X, Y = np.meshgrid(xs, ys)
    R = farrell(np.hypot(X - src_x, Y), mua, musp)
    return R.mean() * (x1 - x0) * (y1 - y0)


s = math.sqrt(7.5) / 2          # 7.5 mm2 square sensitive area per package budget
FULL = (6 - s, 6 + s, -s, s)
SLOT = (5.5, 6.5, -1.25, 1.25)  # 1.0 x 2.5 mm slot centred on PD1 per spec section 3

# IR reference: the package's own near-channel photocurrents at 50 mA
IR_I_FULL = 30.77e-6
IR_I_SLOT = 10.26e-6
IR_PHI = 13.0e-3
IR_RESP = 0.6
IR_MUA, IR_MUSP = 0.019, 0.66   # scalp at ~830 nm, the package's Strangman value

ir_full = collected(*FULL, IR_MUA, IR_MUSP)
ir_slot = collected(*SLOT, IR_MUA, IR_MUSP)
print(f"IR check, diffusion: {ir_full*IR_PHI*1e6:6.1f} uW before window on full PD (package MC, after 2 window passes: 51.29 uW)")
print(f"IR slot/full area-only 0.333, diffusion {ir_slot/ir_full:.3f}")
print(f"HbO2 blood mua at 530 nm = {MUA_BLOOD_530:.1f} /mm [Prahl], skin musp 530 nm = {4.6*(530/500)**-1.421:.2f} /mm [Jacques 2013]\n")

# Green: SFH 7016 true-green bin 2.20-5.40 mW/sr at 20 mA, scaled to 15 mA, Lambertian (phi = pi * Ie)
PHI = {"min": math.pi * 2.20e-3 * 15 / 20, "max": math.pi * 5.40e-3 * 15 / 20}
G_RESP = 0.30                   # A/W at 530 nm [ASSUMED, read the VEMD5080X01 curve]
MUSP_G = 4.6 * (530 / 500) ** -1.421
MEL = {"light fmel 3%": 0.03, "medium fmel 10%": 0.10, "dark fmel 25%": 0.25}
EPI = 0.08                      # epidermis mm [ASSUMED]
AC_G = 0.03                     # pulsatile fraction for green [ASSUMED 2-5 %]


def mel_t(wl, f):
    return math.exp(-2 * EPI * f * 6.6e11 * wl**-3.33 / 10)


def snr(i, ac):
    n = math.sqrt(2 * Q * i * BW + I_AFE_N**2)
    return n, 20 * math.log10(i / n), 20 * math.log10(i * ac / n)


print("Green at 15 mA, min-bin LED, on PD1. Per-sample noise, as in the package table.")
print(f"{'BVF':>5} {'mua':>5} {'skin':>16} | {'I full nA':>9} {'pulse SNR':>9} | {'I slot nA':>9} {'pulse SNR':>9} | {'slot/full':>9}")
for bvf in (0.01, 0.02, 0.04):
    mua = 0.03 + bvf * MUA_BLOOD_530
    for name, f in MEL.items():
        rel = mel_t(530, f) / mel_t(940, f)
        gf = collected(*FULL, mua, MUSP_G)
        gs = collected(*SLOT, mua, MUSP_G)
        # scale from the package's IR photocurrent using the diffusion ratio
        i_full = IR_I_FULL * (gf / ir_full) * (PHI["min"] / IR_PHI) * (G_RESP / IR_RESP) * rel
        i_slot = IR_I_SLOT * (gs / ir_slot) * (PHI["min"] / IR_PHI) * (G_RESP / IR_RESP) * rel
        _, _, af = snr(i_full, AC_G)
        _, _, a_s = snr(i_slot, AC_G)
        print(f"{bvf*100:4.0f}% {mua:5.2f} {name:>16} | {i_full*1e9:9.2f} {af:8.1f}  | {i_slot*1e9:9.2f} {a_s:8.1f}  | {gs/gf:9.2f}")

print("\nSame, IR on the package's slot: 10.26 uA, pulse SNR at 1 % =", round(snr(IR_I_SLOT, 0.01)[2], 1), "dB")
print("Averaging 50 Hz down to a 0.6-4.5 Hz HR band gains", round(10 * math.log10(25 / 3.9), 1), "dB on white noise")

print("\nOption: extra green LED on spare LED4_DRV, centre at x = 5.0 on the same D2-PD1 axis, slot and full cases")
LED_X = 5.0 - 2.2               # position relative to D2, which is the origin of FULL and SLOT
for bvf in (0.01, 0.02, 0.04):
    mua = 0.03 + bvf * MUA_BLOOD_530
    for name, f in (("medium fmel 10%", 0.10), ("dark fmel 25%", 0.25)):
        rel = mel_t(530, f) / mel_t(940, f)
        gf = collected(*FULL, mua, MUSP_G, src_x=LED_X)
        gs = collected(*SLOT, mua, MUSP_G, src_x=LED_X)
        i_full = IR_I_FULL * (gf / ir_full) * (PHI["min"] / IR_PHI) * (G_RESP / IR_RESP) * rel
        i_slot = IR_I_SLOT * (gs / ir_slot) * (PHI["min"] / IR_PHI) * (G_RESP / IR_RESP) * rel
        print(f"  BVF {bvf*100:.0f}% {name:>16}: full {i_full*1e9:9.1f} nA, SNR {snr(i_full, AC_G)[2]:5.1f} dB | slot {i_slot*1e9:9.1f} nA, SNR {snr(i_slot, AC_G)[2]:5.1f} dB")

print("\nOption: extra green LED beside PD1 at (8.2, 3.5), 2.25 mm from the slot end, through the existing slot")
for bvf in (0.01, 0.02, 0.04):
    mua = 0.03 + bvf * MUA_BLOOD_530
    for name, f in (("medium fmel 10%", 0.10), ("dark fmel 25%", 0.25)):
        rel = mel_t(530, f) / mel_t(940, f)
        X, Y = np.meshgrid(np.linspace(SLOT[0], SLOT[1], 400), np.linspace(SLOT[2], SLOT[3], 400))
        gs = farrell(np.hypot(X - 6.0, Y + 3.5), mua, MUSP_G).mean() * (SLOT[1] - SLOT[0]) * (SLOT[3] - SLOT[2])
        i_slot = IR_I_SLOT * (gs / ir_slot) * (PHI["min"] / IR_PHI) * (G_RESP / IR_RESP) * rel
        print(f"  BVF {bvf*100:.0f}% {name:>16}: slot {i_slot*1e9:8.1f} nA, SNR {snr(i_slot, AC_G)[2]:6.1f} dB")

print("\nCase recharge estimate: 300 mAh x 0.90 boost x (3.8 V / 5.0 V linear charger) =", round(300 * 0.90 * 3.8 / 5, 0), "mAh into the pod [ASSUMED efficiencies]")
for name, mah in (("smart", 19.1), ("lite", 12.1)):
    print(f"  {name} at {mah} mAh/day (capability report) -> {300*0.9*3.8/5/mah:.1f} days from the case")
