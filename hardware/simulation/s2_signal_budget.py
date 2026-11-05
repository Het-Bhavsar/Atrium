"""S2. Photocurrent, pulsatile fraction, ADC range and SNR for every sensor option, from the S1 Monte Carlo.

Replaces the package budget, which used one wavelength (830 nm optics with a 950 nm responsivity), a flat window factor,
50 pA AFE noise and an assumed pulsatile fraction. Here each ray goes chip -> window aperture -> adhesive hole -> skin -> R(r)
-> adhesive hole -> window aperture -> PD active area, with Fresnel loss, so aperture size and stand-off are real inputs.

Datasheet inputs: SFH 7016 v1.5 p.5 (Ie, chip size), MAX86174A 19-100999 Rev 1 p.9-10 (ranges, 75 pA at 117.1 us),
VEMD5080X01 Rev 1.4 (7.5 mm2, 0.60 A/W at 950 nm, relative 0.55 and 0.71 at 530 and 660 nm, curve read).

Usage: python3 -I s2_signal_budget.py <s1_mc.json> <out.json>
"""
import json, math, sys
import numpy as np

Q = 1.602e-19
T_INT = 117.1e-6
BW = 1 / (2 * T_INT)
I_AFE = 75e-12
FS_RANGES = (4e-6, 8e-6, 16e-6, 32e-6)
IE_20MA = {530: {"min": 2.2e-3, "typ": 4.0e-3, "max": 5.4e-3},
           660: {"min": 2.5e-3, "typ": 4.2e-3, "max": 7.0e-3},
           940: {"min": 1.65e-3, "typ": 3.0e-3, "max": 4.05e-3}}
RESP = {530: 0.33, 660: 0.43, 940: 0.60}
CHIP_SIZE = {530: 0.5, 660: 0.3, 940: 0.3}
# Chip centres relative to package centre, scaled from the SFH 7016 outline drawing, not dimensioned (+/-0.03 mm)
CHIP = {530: (-0.36, 0.0), 940: (0.26, 0.26), 660: (0.27, -0.27)}
PD_ACTIVE = math.sqrt(7.5)          # 2.74 mm square, assumed centred in the 5 x 4 package
N_PC = 1.58
GAP_LED = 0.40                      # chip surface to window underside: 1.0 skin-side height - 0.6 package height
GAP_PD = 0.15                       # PD top to window underside: 1.0 - 0.85
EPOXY_PD = 0.30                     # active area below PD top surface [ASSUMED]
WIN = 0.50
ADH = 0.25                          # adhesive thickness, an air-filled hole unless skin bulges
SFG = 0.8                           # reduction for pulse RMS relative to peak-to-peak set below


def fresnel_unpol(c_i, n1, n2):
    s_i = np.sqrt(np.maximum(0, 1 - c_i ** 2))
    s_t = n1 / n2 * s_i
    tir = s_t >= 1
    c_t = np.sqrt(np.maximum(1e-12, 1 - s_t ** 2))
    rs = ((n1 * c_i - n2 * c_t) / (n1 * c_i + n2 * c_t)) ** 2
    rp = ((n1 * c_t - n2 * c_i) / (n1 * c_t + n2 * c_i)) ** 2
    T = 1 - 0.5 * (rs + rp)
    return np.where(tir, 0.0, T), c_t


def lambert_dirs(n, rng):
    u = rng.random(n); phi = 2 * np.pi * rng.random(n)
    s = np.sqrt(u); c = np.sqrt(1 - u)
    return s * np.cos(phi), s * np.sin(phi), c


def through_plate(px, py, dx, dy, dz, gap1, win, gap2):
    """Air gap, PC plate, air gap. dz>0. Returns positions at the top of gap1, bottom of plate (two clipping planes) and exit, plus T."""
    t1 = gap1 / dz
    x1, y1 = px + dx * t1, py + dy * t1
    T1, cz = fresnel_unpol(dz, 1.0, N_PC)
    s = np.sqrt(np.maximum(1 - dz ** 2, 0))
    with np.errstate(invalid="ignore", divide="ignore"):
        ux = np.where(s > 0, dx / s, 0); uy = np.where(s > 0, dy / s, 0)
    s_in = s / N_PC
    tan_in = s_in / np.sqrt(1 - s_in ** 2)
    x2, y2 = x1 + ux * win * tan_in, y1 + uy * win * tan_in
    T2, _ = fresnel_unpol(np.sqrt(1 - s_in ** 2), N_PC, 1.0)
    t3 = gap2 / dz
    x3, y3 = x2 + dx * t3, y2 + dy * t3
    return (x1, y1), (x2, y2), (x3, y3), T1 * T2


def led_to_skin(wl, led_xy, aperture_d, hole_d, aperture_c, n, rng, gap_led=GAP_LED, fill=False):
    """Sample where LED light lands on skin. Returns entry points and the throughput fraction."""
    cx, cy = led_xy[0] + CHIP[wl][0], led_xy[1] + CHIP[wl][1]
    a = CHIP_SIZE[wl]
    px = cx + (rng.random(n) - 0.5) * a; py = cy + (rng.random(n) - 0.5) * a
    dx, dy, dz = lambert_dirs(n, rng)
    p1, p2, p3, T = through_plate(px, py, dx, dy, dz, gap_led, WIN, ADH)
    r1 = np.hypot(p1[0] - aperture_c[0], p1[1] - aperture_c[1]) <= aperture_d / 2
    r2 = np.hypot(p2[0] - aperture_c[0], p2[1] - aperture_c[1]) <= aperture_d / 2
    r3 = np.hypot(p3[0] - aperture_c[0], p3[1] - aperture_c[1]) <= hole_d / 2
    keep = r1 & r2 & r3
    frac = float((T * keep).mean())
    return p3[0][keep], p3[1][keep], T[keep], frac


class Rint:
    def __init__(self, mc, wl, case):
        d = mc["wl"][str(wl)]["cases"][case]
        self.nr = mc["nr"]; self.dr = mc["rmax"] / mc["nr"]
        self.rm = (np.arange(self.nr) + 0.5) * self.dr
        R = np.array(d["R_per_mm2"])
        good = R > 0
        self.logR = np.interp(self.rm, self.rm[good], np.log(R[good]))
        self.L = np.array(d["L_mm"])
        self.names = mc["wl"][str(wl)]["layers"]
        self.rel = np.array(d["rel_err"])

    def R(self, r):
        return np.exp(np.interp(r, self.rm, self.logR))

    def Lat(self, r):
        return np.stack([np.interp(r, self.rm, self.L[:, l]) for l in range(self.L.shape[1])], axis=1)


def collect(rint, sx, sy, sT, ap_xy, ap_w, ap_h, hole_extra, pd_xy, n, rng, gap_pd=GAP_PD):
    """Monte Carlo: pick exit points on skin in the adhesive hole, shoot Lambertian rays to the PD active area.
    Returns watts on active area per watt on skin, and the R-weighted partial paths."""
    hw, hh = ap_w / 2 + hole_extra, ap_h / 2 + hole_extra
    ex = ap_xy[0] + (rng.random(n) - 0.5) * 2 * hw; ey = ap_xy[1] + (rng.random(n) - 0.5) * 2 * hh
    area = 4 * hw * hh
    k = rng.integers(0, len(sx), n)
    r = np.hypot(ex - sx[k], ey - sy[k])
    w = rint.R(r) * sT[k] / sT.mean()
    dx, dy, dz = lambert_dirs(n, rng)
    p1, p2, p3, T = through_plate(ex, ey, dx, dy, dz, ADH, WIN, gap_pd + EPOXY_PD)
    ok = (np.abs(p1[0] - ap_xy[0]) <= ap_w / 2) & (np.abs(p1[1] - ap_xy[1]) <= ap_h / 2)
    ok &= (np.abs(p2[0] - ap_xy[0]) <= ap_w / 2) & (np.abs(p2[1] - ap_xy[1]) <= ap_h / 2)
    ok &= (np.abs(p3[0] - pd_xy[0]) <= PD_ACTIVE / 2) & (np.abs(p3[1] - pd_xy[1]) <= PD_ACTIVE / 2)
    weight = w * T * ok
    frac = weight.mean() * area
    # partial paths use the rays that made it
    sel = np.where(ok)[0]
    if len(sel) > 40000:
        sel = sel[:40000]
    L = rint.Lat(r[sel])
    Lw = (L * weight[sel, None]).sum(axis=0) / max(weight[sel].sum(), 1e-30)
    no_clip = (w).mean() * area
    return float(frac), Lw, float(frac / max(no_clip, 1e-30))


def pulse_ac_pp(L, names, wl, scale):
    """Peak-to-peak pulsatile fraction: arterial volume change per layer, with blood at 98 % saturation. The layer pattern is
    [ASSUMED]: small in capillary plexus, largest in the arterial supply of dermis, subcutis and muscle."""
    ext = {530: (39956.8, 39036.4), 660: (319.6, 3226.56), 830: (974.0, 693.04), 940: (1214.0, 693.44)}[wl]
    mb = 2.303 * (0.98 * ext[0] + 0.02 * ext[1]) * (150 / 64500) / 10
    dv = {"papillary": 0.0005, "dermis": 0.0006, "subcutis": 0.0005, "muscle": 0.0006, "skull": 0.0002, "brain": 0.0004}
    return scale * sum(L[names.index(k)] * dv[k] * mb for k in dv)


def motion_dod(L, names, wl):
    """Motion-driven delta OD per unit tissue displacement: venous blood sloshing, weighted to deep compliant layers,
    plus a contact term, relative only. [ASSUMED] ratios, used only to scale artefact between wavelengths."""
    ext = {530: (39956.8, 39036.4), 660: (319.6, 3226.56), 830: (974.0, 693.04), 940: (1214.0, 693.44)}[wl]
    mb = 2.303 * (0.65 * ext[0] + 0.35 * ext[1]) * (150 / 64500) / 10
    e = {"papillary": 0.01, "dermis": 0.03, "subcutis": 0.05, "muscle": 0.08}
    bvf = {"papillary": 0.05, "dermis": 0.015, "subcutis": 0.01, "muscle": 0.04}
    return sum(L[names.index(k)] * e[k] * bvf[k] * mb for k in e)


def noise(i):
    return math.sqrt(2 * Q * i * BW + I_AFE ** 2)


SKIN = ["light", "medium", "dark", "very_dark"]
BVFS = ["low", "nominal", "high"]


def main():
    mc = json.load(open(sys.argv[1]))
    out = sys.argv[2]
    rng = np.random.default_rng(42)

    # sensor options. xy in PCB mm. rot = +1 means the green chip faces PD1 (package rotated 180 deg from CHIP table).
    D2 = (2.2, 7.0)
    PD1 = (8.2, 7.0)
    SLOT = (1.0, 2.5); OPEN = (2.8, 2.8)
    OPT = {
        "A as drawn, green far side, slot": dict(wl=530, led=D2, rot=False, pd=PD1, ap=SLOT, ma=15),
        "B D2 turned, green near side, slot": dict(wl=530, led=D2, rot=True, pd=PD1, ap=SLOT, ma=15),
        "C turned, PD1 window open": dict(wl=530, led=D2, rot=True, pd=PD1, ap=OPEN, ma=15),
        "D turned, open, PD1 at 3.5 mm": dict(wl=530, led=D2, rot=True, pd=(5.7, 7.0), ap=OPEN, ma=15),
        "E PD1 3.5 mm, green at 5 mA": dict(wl=530, led=D2, rot=True, pd=(5.7, 7.0), ap=OPEN, ma=5),
        "F added green LED beside PD1, slot": dict(wl=530, led=(8.2, 3.3), rot=None, pd=PD1, ap=SLOT, ma=15),
        "G added green LED beside PD1, open": dict(wl=530, led=(8.2, 3.3), rot=None, pd=PD1, ap=OPEN, ma=15),
        "H IR near through slot, 50 mA": dict(wl=940, led=D2, rot=False, pd=PD1, ap=SLOT, ma=50),
        "I red near through slot, 40 mA": dict(wl=660, led=D2, rot=False, pd=PD1, ap=SLOT, ma=40),
        "J IR, PD1 open at 3.5 mm, 5 mA": dict(wl=940, led=D2, rot=True, pd=(5.7, 7.0), ap=OPEN, ma=5),
        "K red, PD1 open at 3.5 mm, 5 mA": dict(wl=660, led=D2, rot=True, pd=(5.7, 7.0), ap=OPEN, ma=5),
    }
    FAR = dict(wl=940, led=D2, rot=False, pd=(20.7, 7.0), ap=OPEN, ma=50)

    def evaluate(o, case, bin_="min"):
        wl = o["wl"]
        rint = Rint(mc, wl, case)
        CH = dict(CHIP)
        led = o["led"]
        if o["rot"] is True:
            # package turned so the chip offset points at PD1
            CH[wl] = (abs(CHIP[wl][0]), -CHIP[wl][1])
        elif o["rot"] is None:
            CH[wl] = (0.0, 0.0)
        saved = CHIP[wl]; CHIP[wl] = CH[wl]
        sx, sy, sT, f_led = led_to_skin(wl, led, 1.6, 1.8, led, 200000, rng)
        CHIP[wl] = saved
        f_pd, Lw, vign = collect(rint, sx, sy, sT, o["pd"], o["ap"][0], o["ap"][1], 0.1, o["pd"], 2000000, rng)
        phi = math.pi * IE_20MA[wl][bin_] * o["ma"] / 20.0
        i = phi * f_led * f_pd * RESP[wl]
        return dict(i=i, f_led=f_led, f_pd=f_pd, vign=vign, L=Lw.tolist(), names=rint.names, phi_mW=phi * 1e3)

    # Pulsatile scale: IR near through the slot, medium skin, nominal blood, is set to 1 % peak-to-peak, the middle of the
    # package's assumed 0.5-2 %, and everything else follows from the partial paths.
    ref = evaluate(OPT["H IR near through slot, 50 mA"], "medium|nominal")
    scale = 0.01 / pulse_ac_pp(ref["L"], ref["names"], 940, 1.0)
    R = {"ac_scale": scale, "options": {}, "far": {}, "range": {}, "inputs": {"i_afe_pA": I_AFE * 1e12, "t_int_us": T_INT * 1e6}}
    print(f"pulsatile scale {scale:.3f} (1.0 means the assumed arterial pattern already gives 1 % at IR)")
    print(f"LED chip throughput to skin, Oe1.6 aperture on package centre: ", end="")
    for wl in (530, 660, 940):
        print(f"{wl}: {led_to_skin(wl, D2, 1.6, 1.8, D2, 200000, rng)[3]:.2f}  ", end="")
    print()

    print("\nPhotocurrent at MIN LED bin. AC = peak-to-peak pulse fraction. SNR_s = per-sample RMS pulse / noise. SNR_b = after the HR band (+8 dB)")
    print(f"{'option':<38} {'skin':<10}{'BVF':<8}{'I':>10}{'AC %':>7}{'SNR_s':>7}{'SNR_b':>7}{'L_derm':>7}{'L_musc':>7}")
    for name, o in OPT.items():
        R["options"][name] = {}
        for sk in SKIN:
            for bv in BVFS:
                c = f"{sk}|{bv}"
                e = evaluate(o, c)
                ac = pulse_ac_pp(np.array(e["L"]), e["names"], o["wl"], scale)
                n = noise(e["i"])
                snr = 20 * math.log10(max(e["i"] * ac / 3.0, 1e-30) / n)
                mot = motion_dod(np.array(e["L"]), e["names"], o["wl"])
                R["options"][name][c] = dict(i=e["i"], ac=ac, snr_s=snr, snr_b=snr + 8.1, L=e["L"], f_led=e["f_led"], f_pd=e["f_pd"], vign=e["vign"], motion=mot)
                if (sk, bv) in (("medium", "nominal"), ("very_dark", "nominal"), ("medium", "high"), ("very_dark", "high"), ("light", "low")):
                    L = np.array(e["L"]); nm = e["names"]
                    print(f"{name:<38} {sk:<10}{bv:<8}{e['i']*1e9:9.2f}n{ac*100:7.2f}{snr:7.1f}{snr+8.1:7.1f}{L[nm.index('dermis')]:7.2f}{L[nm.index('muscle')]:7.2f}")

    # Far channel at 940 nm
    print("\nFar channel, IR 50 mA, min bin, full 2.8 mm window")
    for c in ("light|nominal", "medium|nominal", "dark|nominal", "very_dark|nominal"):
        e = evaluate(FAR, c)
        L = np.array(e["L"]); nm = e["names"]
        ac = pulse_ac_pp(L, nm, 940, scale)
        R["far"][c] = dict(i=e["i"], L=e["L"], names=nm, ac=ac, snr_dc=20 * math.log10(e["i"] / noise(e["i"])))
        print(f"  {c:<18} I {e['i']*1e9:8.1f} nA  dc SNR {R['far'][c]['snr_dc']:.1f} dB  brain path {L[nm.index('brain')]:.2f} of {L.sum():.1f} mm ({100*L[nm.index('brain')]/L.sum():.1f} %)  muscle {100*L[nm.index('muscle')]/L.sum():.0f} %  skull {100*L[nm.index('skull')]/L.sum():.0f} %")
    # 830 versus 940: the package ran the former
    print("\nPackage wavelength check, Monte Carlo reflectance ratio 940/830 at medium skin:")
    for r0 in (6.0, 18.5):
        a = Rint(mc, 940, "medium|nominal"); b = Rint(mc, 830, "medium|nominal")
        ratio = float(a.R(np.array([r0]))[0] / b.R(np.array([r0]))[0])
        print(f"  r {r0} mm: {ratio:.2f}")
        R[f"r940_830_{r0}"] = ratio

    # ADC range check at MAX bin, lightest skin, low blood: the case that saturates first.
    print("\nADC range check, max LED bin, light skin, low blood. Package drives one IR pulse at 50 mA for both PDs.")
    for label, o in (("PD1 slot, IR 50 mA", OPT["H IR near through slot, 50 mA"]),
                     ("PD1 open 6 mm, IR 50 mA", dict(OPT["H IR near through slot, 50 mA"], ap=OPEN)),
                     ("PD1 open 3.5 mm, IR 50 mA", dict(OPT["H IR near through slot, 50 mA"], ap=OPEN, pd=(5.7, 7.0), rot=True)),
                     ("PD1 open 3.5 mm, IR 5 mA", OPT["J IR, PD1 open at 3.5 mm, 5 mA"]),
                     ("PD1 open 3.5 mm, red 5 mA", OPT["K red, PD1 open at 3.5 mm, 5 mA"]),
                     ("PD1 open 3.5 mm, green 15 mA", OPT["D turned, open, PD1 at 3.5 mm"])):
        e = evaluate(o, "light|low", "max")
        fit = next((f for f in FS_RANGES if e["i"] < 0.8 * f), None)
        print(f"  {label:<30} {e['i']*1e6:8.2f} uA  range {'SATURATES 32 uA' if fit is None else f'{fit*1e6:.0f} uA'}")
        R["range"][label] = dict(i=e["i"], fit=fit)

    # Crosstalk: how much LED light may leak directly into the PD before it matters
    print("\nAllowed direct LED-to-PD leakage (as a fraction of LED flux), green 15 mA min bin:")
    phi = math.pi * IE_20MA[530]["min"] * 15 / 20
    full = phi * 0.33
    shot_lim = I_AFE ** 2 / (2 * Q * BW)
    print(f"  shot noise from leakage equals AFE noise at {shot_lim*1e6:.1f} uA, fraction {shot_lim/full:.1e} ({10*math.log10(shot_lim/full):.0f} dB)")
    R["leak_shot_uA"] = shot_lim * 1e6
    for name in ("A as drawn, green far side, slot", "C turned, PD1 window open", "D turned, open, PD1 at 3.5 mm"):
        i = R["options"][name]["medium|nominal"]["i"]
        print(f"  equal to the tissue signal of {name}: {i/full:.1e} ({10*math.log10(i/full):.0f} dB)")

    # HR options for S3. Motion ratio per option comes from partial paths, normalised to option D green.
    hr = {}
    ref_ratio = R["options"]["D turned, open, PD1 at 3.5 mm"]["medium|nominal"]
    ref_ratio = ref_ratio["motion"] / ref_ratio["ac"]
    for name in OPT:
        for sk in ("medium", "very_dark"):
            r = R["options"][name][f"{sk}|nominal"]
            hr[f"{name} [{sk}]"] = dict(i_dc=r["i"], ac=r["ac"] / 3.0, motion_scale=(r["motion"] / r["ac"]) / ref_ratio)
    R["hr_options"] = hr
    R["hr_ref_option"] = "D turned, open, PD1 at 3.5 mm [medium]"
    print("\nArtefact per unit pulse, relative to green at 3.5 mm, from the partial paths:")
    for k, v in hr.items():
        if "[medium]" in k:
            print(f"  {k:<48} {v['motion_scale']:.2f}")

    # Blood sensitivity envelope and SDS sweep for green and IR on a bare PD, with the same stack
    print("\nDistance sweep, green and IR on the open 2.8 mm window, medium skin, chip-to-PD centre")
    R["sds"] = {}
    for wl, ma, bn in ((530, 15, "min"), (940, 5, "min"), (660, 5, "min")):
        R["sds"][wl] = {}
        rint_name = "medium|nominal"
        for d in (3.0, 3.5, 4.0, 5.0, 6.0, 7.0, 8.0):
            rint = Rint(mc, wl, rint_name)
            saved = CHIP[wl]; CHIP[wl] = (0.0, 0.0)
            sx, sy, sT, f_led = led_to_skin(wl, D2, 1.6, 1.8, D2, 100000, rng)
            CHIP[wl] = saved
            f_pd, Lw, _ = collect(rint, sx, sy, sT, (D2[0] + d, 7.0), 2.8, 2.8, 0.1, (D2[0] + d, 7.0), 1000000, rng)
            phi = math.pi * IE_20MA[wl][bn] * ma / 20.0
            i = phi * f_led * f_pd * RESP[wl]
            ac = pulse_ac_pp(Lw, rint.names, wl, scale)
            snr = 20 * math.log10(max(i * ac / 3, 1e-30) / noise(i))
            nm = rint.names; tot = Lw.sum()
            R["sds"][wl][d] = dict(i=i, ac=ac, snr=snr, L=Lw.tolist())
            print(f"  {wl} nm {ma} mA d {d:4.1f} mm  I {i*1e9:9.1f} nA  AC {ac*100:5.2f}%  SNR_s {snr:5.1f} dB   path: epi+derm {100*(Lw[0]+Lw[1]+Lw[2])/tot:3.0f}%  subcut {100*Lw[3]/tot:3.0f}%  muscle {100*Lw[4]/tot:3.0f}%")
    json.dump(R, open(out, "w"), indent=1, default=float)


if __name__ == "__main__":
    main()
