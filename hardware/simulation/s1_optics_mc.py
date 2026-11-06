"""S1. Layered Monte Carlo photon transport for the Atrium pod at the wavelengths the LEDs really emit.

SFH 7016 emits 530, 660 and 940 nm, so this runs each with wavelength-specific absorption and
scattering (830 nm is kept as a reference). Each run reweights one photon history to many blood-volume and
melanin cases ("perturbation" Monte Carlo), so all cases share photon noise and compare cleanly.

Usage (from the repo root):
       python3 hardware/simulation/s1_optics_mc.py 4e7 hardware/simulation/results/s1_mc.json
       python3 hardware/simulation/s1_optics_mc.py 1e7 hardware/simulation/results/s1_mc_lowscat.json --scat=0.5 --wl=530,940
"""
import json, math, sys, time
import numpy as np
from numba import njit, prange

# Prahl hemoglobin tabulation (omlc.org/spectra/hemoglobin/summary.html), cm^-1/M, ("HbO2", "Hb").
EXT = {530: (39956.8, 39036.4), 660: (319.6, 3226.56), 830: (974.0, 693.04), 940: (1214.0, 693.44)}
HB_M = 150.0 / 64500.0            # whole blood, 150 g/L
SAT_ART, SAT_TISSUE = 0.98, 0.70  # arterial and mixed tissue saturation [ASSUMED]
WATER_940, WATER_660, WATER_530, WATER_830 = 0.027, 0.0004, 0.00004, 0.0028  # /mm pure water, Hale & Querry order
WATER = {530: WATER_530, 660: WATER_660, 830: WATER_830, 940: WATER_940}


def mua_blood(wl, sat):
    o, d = EXT[wl]
    return 2.303 * (sat * o + (1 - sat) * d) * HB_M / 10.0   # /mm


def mua_melanin(wl):
    return 519.0 * (wl / 500.0) ** -3.0 / 10.0  # /mm, Jacques 2013 Eq. 8, m = 3, melanosome interior


def mus_prime(tissue, wl):
    """Reduced scattering /mm. Jacques 2013 Eq. 1 with a, b from Table 1 and 2: skin, fatty, muscle (Tromberg), bone, brain."""
    a, b = {"skin": (46.0, 1.421), "fat": (18.4, 0.672), "muscle": (13.0, 0.926),
            "bone": (22.9, 0.716), "brain": (24.2, 1.611), "csf": (1.0, 0.0)}[tissue]
    return a * (wl / 500.0) ** -b / 10.0


# Layer table for the side of the head where the pod sits. Thicknesses [ASSUMED]: 3 mm scalp + 7 mm muscle.
# Each: (name, thickness mm, tissue for scattering, blood fraction base, water fraction, extra mua /mm, n)
def layers(wl, bvf_scale=1.0, f_mel=0.0, muscle_mm=7.0):
    g = 0.9
    t = [("epidermis", 0.08, "skin", 0.0, 0.6, 0.0),
         ("papillary", 0.15, "skin", 0.05, 0.6, 0.0),
         ("dermis", 1.30, "skin", 0.015, 0.65, 0.0),
         ("subcutis", 1.47, "fat", 0.01, 0.2, 0.0),
         ("muscle", muscle_mm, "muscle", 0.04, 0.75, 0.0),
         ("skull", 3.0, "bone", 0.03, 0.2, 0.0),
         ("csf", 2.0, "csf", 0.0, 0.99, 0.0),
         ("brain", 40.0, "brain", 0.04, 0.8, 0.0)]
    bg = 0.0025  # /mm, bloodless tissue background [ASSUMED, Jacques 2013 skin baseline]
    out = []
    for name, d, tissue, bvf, w, _ in t:
        mb = mua_blood(wl, 0.8 * SAT_TISSUE + 0.2 * SAT_ART)
        m = bg + bvf * bvf_scale * mb + w * WATER[wl]
        if name == "epidermis":
            m += f_mel * mua_melanin(wl)
        out.append((name, d, m, mus_prime(tissue, wl) / (1 - g)))
    return out, g


RMAX, NR = 40.0, 160
DR = RMAX / NR


@njit(parallel=True, cache=True)
def run(nphot, zb, mua0, mus, g, dmua, seed):
    """dmua[k, l]: absorption of case k minus base absorption mua0 in layer l. Tallies detected weight and
    partial pathlength per ring and case. w stays the base weight, each case multiplies exp(-sum dmua*L)."""
    nl = mua0.shape[0]
    nk = dmua.shape[0]
    nthreads = 44
    W = np.zeros((nthreads, nk, NR))
    W2 = np.zeros((nthreads, nk, NR))
    WL = np.zeros((nthreads, nk, NR, nl))
    per = nphot // nthreads
    for t in prange(nthreads):
        np.random.seed(seed + t * 7919)
        lp = np.zeros(nl)
        for p in range(per):
            x = 0.0; y = 0.0; z = 0.0; ux = 0.0; uy = 0.0; uz = 1.0
            w = 1.0; lay = 0
            lp[:] = 0.0
            alive = True
            while alive:
                s = -math.log(np.random.random() + 1e-300)
                while s > 0.0 and alive:
                    step = s / mus[lay]
                    if uz > 0.0:
                        db = (zb[lay + 1] - z) / uz
                    elif uz < 0.0:
                        db = (zb[lay] - z) / uz
                    else:
                        db = 1e30
                    if step >= db:
                        stp = db; s -= db * mus[lay]
                    else:
                        stp = step; s = 0.0
                    lp[lay] += stp
                    w *= math.exp(-mua0[lay] * stp)
                    x += ux * stp; y += uy * stp; z += uz * stp
                    if s > 0.0:
                        if uz < 0.0:
                            if lay == 0:
                                r = math.sqrt(x * x + y * y)
                                ir = int(r / (RMAX / NR))
                                if ir < NR:
                                    for k in range(nk):
                                        e = 0.0
                                        for l in range(nl):
                                            e += dmua[k, l] * lp[l]
                                        wk = w * math.exp(-e)
                                        W[t, k, ir] += wk
                                        W2[t, k, ir] += wk * wk
                                        for l in range(nl):
                                            WL[t, k, ir, l] += wk * lp[l]
                                alive = False
                            else:
                                lay -= 1; z = zb[lay + 1] - 1e-9
                        else:
                            if lay == nl - 1:
                                alive = False
                            else:
                                lay += 1; z = zb[lay] + 1e-9
                if not alive:
                    break
                if x * x + y * y > 60.0 * 60.0:
                    break
                if w < 1e-7:
                    if np.random.random() < 0.1:
                        w *= 10.0
                    else:
                        break
                xi = np.random.random()
                tmp = (1.0 - g * g) / (1.0 - g + 2.0 * g * xi)
                ct = (1.0 + g * g - tmp * tmp) / (2.0 * g)
                ct = min(1.0, max(-1.0, ct))
                st = math.sqrt(1.0 - ct * ct)
                phi = 2.0 * math.pi * np.random.random()
                cp = math.cos(phi); sp = math.sin(phi)
                if abs(uz) > 0.99999:
                    nux = st * cp; nuy = st * sp; nuz = ct * (1.0 if uz > 0 else -1.0)
                else:
                    d = math.sqrt(1.0 - uz * uz)
                    nux = st * (ux * uz * cp - uy * sp) / d + ux * ct
                    nuy = st * (uy * uz * cp + ux * sp) / d + uy * ct
                    nuz = -st * cp * d + uz * ct
                ux = nux; uy = nuy; uz = nuz
    return W.sum(axis=0), W2.sum(axis=0), WL.sum(axis=0), per * nthreads


def farrell_ring(rmid, mua, musp, n=1.0):
    """Farrell 1992 reflectance /mm2, index-matched, for the homogeneous validation case."""
    mut = mua + musp; a = musp / mut; mueff = math.sqrt(3 * mua * mut); D = 1 / (3 * mut)
    z0 = 1 / mut; zb = 2 * D
    r1 = np.sqrt(z0 ** 2 + rmid ** 2); r2 = np.sqrt((z0 + 2 * zb) ** 2 + rmid ** 2)
    return a / (4 * math.pi) * (z0 * (mueff + 1 / r1) * np.exp(-mueff * r1) / r1 ** 2
                                + (z0 + 2 * zb) * (mueff + 1 / r2) * np.exp(-mueff * r2) / r2 ** 2)


def validate(n):
    """Homogeneous slab against Farrell: the MC engine must land on the analytic curve."""
    mua = np.array([0.02]); mus = np.array([10.0]); zb = np.array([0.0, 200.0])
    W, _, _, N = run(n, zb, mua, mus, 0.9, np.zeros((1, 1)), 7)
    area = math.pi * ((np.arange(NR) + 1) ** 2 - np.arange(NR) ** 2) * DR ** 2
    rm = (np.arange(NR) + 0.5) * DR
    R = W[0] / N / area
    F = farrell_ring(rm, 0.02, 1.0)
    for rr in (3, 6, 10, 18.5):
        i = int(rr / DR)
        print(f"  r={rr:5.1f} mm  MC {R[i]:.3e}  Farrell {F[i]:.3e}  ratio {R[i]/F[i]:.3f}")


def main():
    n = int(float(sys.argv[1]))
    scat = next((float(a.split("=")[1]) for a in sys.argv if a.startswith("--scat=")), 1.0)
    wls = next((tuple(int(x) for x in a.split("=")[1].split(",")) for a in sys.argv if a.startswith("--wl=")), (530, 660, 940, 830))
    if "--validate" in sys.argv:
        validate(n)
        return
    res = {"n_photons": n, "rmax": RMAX, "nr": NR, "wl": {}}
    MEL = {"light": 0.03, "medium": 0.10, "dark": 0.25, "very_dark": 0.40}  # epidermal melanosome fraction, Jacques
    BVF = {"low": 0.5, "nominal": 1.0, "high": 2.0}
    for wl in wls:
        base, g = layers(wl, 0.5, 0.0)
        names = [b[0] for b in base]
        zb = np.concatenate([[0.0], np.cumsum([b[1] for b in base])])
        mua0 = np.array([b[2] for b in base]); mus = np.array([b[3] for b in base]) * scat
        cases, dm = [], []
        for mn, mf in MEL.items():
            for bn, bs in BVF.items():
                lay, _ = layers(wl, bs, mf)
                dm.append(np.array([b[2] for b in lay]) - mua0)
                cases.append(f"{mn}|{bn}")
        t0 = time.time()
        W, W2, WL, N = run(n, zb, mua0, mus, g, np.array(dm), 1234 + wl)
        area = math.pi * ((np.arange(NR) + 1) ** 2 - np.arange(NR) ** 2) * DR ** 2
        rows = {}
        for k, c in enumerate(cases):
            with np.errstate(divide="ignore", invalid="ignore"):
                L = np.where(W[k][:, None] > 0, WL[k] / W[k][:, None], 0.0)
                rel = np.sqrt(np.where(W[k] > 0, W2[k], 0.0)) / np.where(W[k] > 0, W[k], 1.0)
            rows[c] = {"R_per_mm2": (W[k] / N / area).tolist(), "rel_err": rel.tolist(), "L_mm": L.tolist()}
        res["wl"][str(wl)] = {"layers": names, "thick": [b[1] for b in base], "mua_base": mua0.tolist(),
                              "mus": mus.tolist(), "cases": rows}
        i6 = int(6 / DR); c = "medium|nominal"
        print(f"wl {wl}: {time.time()-t0:.0f}s  R(6mm) {rows[c]['R_per_mm2'][i6]:.3e}  rel err {rows[c]['rel_err'][i6]:.3f}", flush=True)
    json.dump(res, open(sys.argv[2], "w"))


if __name__ == "__main__":
    main()
