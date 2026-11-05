"""S6. 3D finite-difference thermal model of the pod on skin: steady state and transient.

Answers three questions the lumped estimate could not:
 1. How far does the TMP117 reading sit from true skin temperature, and how much do electronics disturb it?
 2. Does U4 minus TH1 (the package's "heat-flux reference") actually track heat leaving the skin?
 3. Which sensor position would, and how much do the U4 slots help?

Conductivities are [ASSUMED] handbook values, see K below. Skin is a Pennes bioheat layer over core-temperature tissue.
Usage: python3 -I s6_thermal.py <pod_placement.csv> <out.json>
"""
import csv, json, sys
import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as spl

# Conductivity W/mK, volumetric heat capacity J/m3K
K = {"air_gap": (0.03, 1.2e3), "tissue": (0.40, 3.6e6), "adhesive": (0.25, 1.8e6), "pc": (0.20, 1.5e6),
     "parts": (0.8, 2.0e6), "pcb_xy": (45.0, 2.4e6), "pcb_z": (0.6, 2.4e6), "cell_xy": (15.0, 2.5e6), "cell_z": (1.5, 2.5e6),
     "foam": (0.05, 1.5e5), "steel": (15.0, 3.9e6), "gap_pad": (1.5, 2.0e6)}
PERF = 2.0e3     # W/m3K, resting scalp perfusion: w*rho*c ~ 0.5 mL/g/min equivalent [ASSUMED]
T_CORE = 37.0
NZ_SKIN = 10

# Pod stack from skin (z=0) up, spec section 3. dz chosen to keep the thin layers resolved.
STACK = [("adhesive", 0.25, 1), ("window", 0.50, 2), ("skinparts", 1.00, 2), ("pcb", 0.60, 2), ("topparts", 0.90, 2),
         ("cell", 3.00, 3), ("foam", 0.30, 1), ("shell", 0.50, 1)]


def stadium(x, y, L=30.0, W=16.0):
    """Inside the pod outer stadium? Coordinates are pod-centred mm."""
    R = W / 2
    if abs(x) <= L / 2 - R:
        return abs(y) <= R
    cx = (L / 2 - R) * np.sign(x)
    return (x - cx) ** 2 + y ** 2 <= R ** 2


class Model:
    def __init__(self, placement, slots=True, h_top=10.0, tamb=25.0, th1=None, hx=0.5e-3):
        self.P = {r["ref"]: r for r in csv.DictReader(open(placement))}
        self.h = h_top; self.tamb = tamb; self.slots = slots; self.hx = hx
        self.th1 = th1
        margin = 8.0  # mm of skin around the pod
        self.xs = np.arange(-15 - margin, 15 + margin + 1e-6, hx * 1e3)
        self.ys = np.arange(-8 - margin, 8 + margin + 1e-6, hx * 1e3)
        self.build()

    def pcb_xy(self, xp, yp):
        """Pod-centred coordinates of PCB coordinates. The PCB is 28 x 14, the pod 30 x 16, both centred."""
        return xp - 14.0, yp - 7.0

    def build(self):
        zs, labels = [], []
        for i in range(NZ_SKIN):        # tissue, graded, coarse deep down, 8 mm total
            pass
        tz = np.geomspace(0.15, 1.8, NZ_SKIN)
        tz *= 8.0 / tz.sum()
        for d in tz[::-1]:
            zs.append(d); labels.append("tissue")
        for name, thick, n in STACK:
            for _ in range(n):
                zs.append(thick / n); labels.append(name)
        for _ in range(2):              # air layer idea replaced by a surface film, two thin voxels of nothing are skipped
            pass
        self.dz = np.array(zs) * 1e-3
        self.lab = labels
        nx, ny, nz = len(self.xs), len(self.ys), len(zs)
        self.nx, self.ny, self.nz = nx, ny, nz
        kx = np.full((nx, ny, nz), 0.0); kz = np.zeros_like(kx); cv = np.zeros_like(kx)
        inside = np.zeros((nx, ny), bool)
        for i, x in enumerate(self.xs):
            for j, y in enumerate(self.ys):
                inside[i, j] = stadium(x, y)
        self.inside = inside
        for k, name in enumerate(labels):
            if name == "tissue":
                kx[:, :, k] = K["tissue"][0]; kz[:, :, k] = K["tissue"][0]; cv[:, :, k] = K["tissue"][1]; continue
            mat = {"adhesive": ("adhesive",) * 2, "window": ("pc",) * 2, "skinparts": ("air_gap",) * 2, "pcb": ("pcb_xy", "pcb_z"),
                   "topparts": ("air_gap",) * 2, "cell": ("cell_xy", "cell_z"), "foam": ("foam",) * 2, "shell": ("pc",) * 2}[name]
            kx[:, :, k][inside] = K[mat[0]][0]; kz[:, :, k][inside] = K[mat[1]][0]; cv[:, :, k][inside] = K[mat[1]][1]
        # the PCB stadium is smaller than the shell, the 1 mm wall is PC
        for k, name in enumerate(labels):
            if name in ("skinparts", "pcb", "topparts", "cell"):
                for i, x in enumerate(self.xs):
                    for j, y in enumerate(self.ys):
                        if inside[i, j] and not stadium(x, y, 28.0, 14.0):
                            kx[i, j, k] = K["pc"][0]; kz[i, j, k] = K["pc"][0]; cv[i, j, k] = K["pc"][1]
        # components: solid blocks in the skin-side and top-side air gaps
        zlay = {n: [k for k, m in enumerate(labels) if m == n] for n in set(labels)}
        self.src = {}
        for ref, r in self.P.items():
            x0, y0 = self.pcb_xy(float(r["x_mm"]), float(r["y_mm"])); w, h = float(r["w_mm"]), float(r["h_mm"])
            ii = np.where((self.xs >= x0 - w / 2) & (self.xs <= x0 + w / 2))[0]
            jj = np.where((self.ys >= y0 - h / 2) & (self.ys <= y0 + h / 2))[0]
            if len(ii) == 0: ii = np.array([np.argmin(abs(self.xs - x0))])
            if len(jj) == 0: jj = np.array([np.argmin(abs(self.ys - y0))])
            klist = zlay["skinparts"] if r["side"] == "skin" else zlay["topparts"]
            hgt = float(r["max_height_mm"])
            if hgt < 0.2:
                continue
            for k in klist:
                for a in ii:
                    for b in jj:
                        kx[a, b, k] = K["parts"][0]; kz[a, b, k] = K["parts"][0]; cv[a, b, k] = K["parts"][1]
            self.src[ref] = (ii, jj, klist)
        # thermal path above U4: Oe2.5 steel button through the window, gap pad to the package
        xu, yu = self.pcb_xy(14.0, 7.0)
        btn = [(a, b) for a, x in enumerate(self.xs) for b, y in enumerate(self.ys) if (x - xu) ** 2 + (y - yu) ** 2 <= 1.25 ** 2]
        for a, b in btn:
            for k in zlay["window"]:
                kx[a, b, k] = K["steel"][0]; kz[a, b, k] = K["steel"][0]; cv[a, b, k] = K["steel"][1]
        # isolation slots: three sides of the U4 island through the PCB, 0.3 mm wide. At 0.5 mm grid this is one voxel of air.
        if self.slots:
            w = 1.0 + 0.25 + 0.15
            for k in zlay["pcb"]:
                for a, x in enumerate(self.xs):
                    for b, y in enumerate(self.ys):
                        dx, dy = x - xu, y - yu
                        on_left = abs(dx + w) <= 0.25 and -w <= dy <= w
                        on_right = abs(dx - w) <= 0.25 and -w <= dy <= w
                        on_bottom = abs(dy + w) <= 0.25 and -w <= dx <= w
                        if on_left or on_right or on_bottom:
                            kx[a, b, k] = K["air_gap"][0]; kz[a, b, k] = K["air_gap"][0]; cv[a, b, k] = K["air_gap"][1]
        self.kx, self.kz, self.cv = kx, kz, cv
        self._assemble()

    def _id(self, i, j, k):
        return i + self.nx * (j + self.ny * k)

    def _assemble(self):
        nx, ny, nz = self.nx, self.ny, self.nz
        dx = self.hx
        dz = self.dz
        kx, kz = self.kx, self.kz
        N = nx * ny * nz
        I = np.arange(N).reshape((nx, ny, nz), order="F")
        rows, cols, vals = [], [], []
        diag = np.zeros(N); rhs_coef = np.zeros(N); rhs_amb = np.zeros(N)

        def harm(a, b, da, db):
            with np.errstate(divide="ignore", invalid="ignore"):
                return np.where((a > 0) & (b > 0), (da + db) / (da / np.maximum(a, 1e-12) + db / np.maximum(b, 1e-12)), 0.0)

        vol = (dx * dx * dz)[None, None, :] * np.ones((nx, ny, nz))
        # x and y couplings
        for axis in (0, 1):
            a = kx.take(range(0, kx.shape[axis] - 1), axis=axis); b = kx.take(range(1, kx.shape[axis]), axis=axis)
            k_h = harm(a, b, dx, dx)
            ia = I.take(range(0, I.shape[axis] - 1), axis=axis); ib = I.take(range(1, I.shape[axis]), axis=axis)
            G = (k_h * (dx * dz[None, None, :]) / dx).ravel(order="F")
            ia, ib = ia.ravel(order="F"), ib.ravel(order="F")
            m = G > 0
            rows += [ia[m], ib[m]]; cols += [ib[m], ia[m]]; vals += [-G[m], -G[m]]
            np.add.at(diag, ia[m], G[m]); np.add.at(diag, ib[m], G[m])
        # z couplings
        a = kz[:, :, :-1]; b = kz[:, :, 1:]
        dza = dz[:-1][None, None, :]; dzb = dz[1:][None, None, :]
        with np.errstate(divide="ignore", invalid="ignore"):
            res = np.where((a > 0) & (b > 0), dza / 2 / np.maximum(a, 1e-12) + dzb / 2 / np.maximum(b, 1e-12), np.inf)
        G = (dx * dx / res).ravel(order="F")
        ia, ib = I[:, :, :-1].ravel(order="F"), I[:, :, 1:].ravel(order="F")
        m = G > 0
        rows += [ia[m], ib[m]]; cols += [ib[m], ia[m]]; vals += [-G[m], -G[m]]
        np.add.at(diag, ia[m], G[m]); np.add.at(diag, ib[m], G[m])
        # tissue: perfusion and a fixed core at the deepest plane
        tissue_k = [k for k, n in enumerate(self.lab) if n == "tissue"]
        for k in tissue_k:
            g = PERF * dx * dx * dz[k]
            diag[I[:, :, k].ravel(order="F")] += g
            rhs_coef[I[:, :, k].ravel(order="F")] += g * T_CORE
        g_core = 0.40 * dx * dx / (dz[0] / 2)
        diag[I[:, :, 0].ravel(order="F")] += g_core
        rhs_coef[I[:, :, 0].ravel(order="F")] += g_core * T_CORE
        # top surface of tissue outside the pod: convection and sweat-free radiation lumped into h
        top = tissue_k[-1]
        outside = ~self.inside
        A = dx * dx
        h_skin = self.h
        idx = I[:, :, top][outside]
        diag[idx] += h_skin * A; rhs_amb[idx] += h_skin * A
        # pod surfaces: top face and side wall
        topk = nz - 1
        idx = I[:, :, topk][self.inside]
        diag[idx] += self.h * A; rhs_amb[idx] += self.h * A
        # side walls: voxels inside the stadium with a neighbour outside, above the tissue
        ins = self.inside
        edge = ins & ~(np.roll(ins, 1, 0) & np.roll(ins, -1, 0) & np.roll(ins, 1, 1) & np.roll(ins, -1, 1))
        for k in range(top + 1, nz):
            g = self.h * dx * dz[k]
            idx = I[:, :, k][edge]
            diag[idx] += g; rhs_amb[idx] += g
        # voxels with no conduction at all are isolated, pin them
        solid = (kx > 0).ravel(order="F")
        diag[~solid] = 1.0; rhs_amb[~solid] = 1.0
        rows.append(np.arange(N)); cols.append(np.arange(N)); vals.append(diag)
        self.A = sp.csc_matrix((np.concatenate(vals), (np.concatenate(rows), np.concatenate(cols))), shape=(N, N))
        self.b_core = rhs_coef; self.b_amb = rhs_amb
        self.I = I
        self.vol = vol
        self.lu = None

    def _power(self, Q):
        b = np.zeros(self.nx * self.ny * self.nz)
        for ref, watts in Q.items():
            if ref not in self.src or watts == 0:
                continue
            ii, jj, kl = self.src[ref]
            ids = self.I[np.ix_(ii, jj, kl)].ravel(order="F")
            b[ids] += watts / len(ids)
        return b

    def steady(self, Q, tamb=None):
        if tamb is not None:
            self.tamb = tamb
        b = self.b_core + self.b_amb * self.tamb + self._power(Q)
        if self.hx >= 1e-3:
            if self.lu is None:
                self.lu = spl.factorized(self.A)
            return self.lu(b).reshape((self.nx, self.ny, self.nz), order="F")
        # the fine grid is too big for a direct solve: conjugate gradient with an incomplete factorisation, warm-started
        if self.lu is None:
            ilu = spl.spilu(self.A.tocsc(), drop_tol=1e-3, fill_factor=8)
            self.lu = spl.LinearOperator(self.A.shape, ilu.solve)
            self.x0 = np.full(self.A.shape[0], 30.0)
        x, info = spl.cg(self.A, b, x0=self.x0, M=self.lu, tol=1e-7, atol=0, maxiter=4000)
        self.x0 = x
        if info != 0:
            print("  CG did not converge", info, flush=True)
        return x.reshape((self.nx, self.ny, self.nz), order="F")

    def transient(self, Q_fn, T0, dt, nsteps):
        """Implicit Euler. Q_fn(step)->dict of watts; b0 is fixed, so ambient steps need another Model."""
        cap = (self.cv * self.vol).ravel(order="F")
        cap = np.maximum(cap, 1e-9)
        M = (sp.diags(cap / dt) + self.A).tocsc()
        lu = spl.factorized(M)
        T = T0.ravel(order="F").copy()
        out = []
        for s in range(nsteps):
            b = self.b_core + self.b_amb * self.tamb + self._power(Q_fn(s)) + cap / dt * T
            T = lu(b)
            out.append(T.copy())
        return [x.reshape((self.nx, self.ny, self.nz), order="F") for x in out]

    def ix(self, xp, yp):
        x, y = self.pcb_xy(xp, yp)
        return np.argmin(abs(self.xs - x)), np.argmin(abs(self.ys - y))

    def probes(self, T, th1=(0.9, 7.0)):
        lab = self.lab
        ks = [k for k, n in enumerate(lab) if n == "skinparts"]
        kp = [k for k, n in enumerate(lab) if n == "pcb"]
        kt = [k for k, n in enumerate(lab) if n == "topparts"]
        ktop = len(lab) - 1
        kskin = [k for k, n in enumerate(lab) if n == "tissue"][-1]
        a, b = self.ix(14.0, 7.0)
        c, d = self.ix(*th1)
        e, f = self.ix(14.0, 7.0)
        ins = self.inside
        cx, cy = len(self.xs) // 2, len(self.ys) // 2
        return {"skin_under_U4": T[a, b, kskin], "U4": T[a, b, ks[len(ks) // 2]], "TH1": T[c, d, kt[0]],
                "pcb_centre": T[cx, cy, kp[0]], "shell_top_centre": T[cx, cy, ktop],
                "cell_mid": T[cx, cy, [k for k, n in enumerate(lab) if n == "cell"][1]],
                "skin_max_under_pod": T[:, :, kskin][ins].max(), "skin_min_under_pod": T[:, :, kskin][ins].min(),
                "skin_mean_under_pod": T[:, :, kskin][ins].mean()}

    def flux_through_top(self, T):
        """Total heat leaving the pod top and walls by convection, W."""
        k = len(self.lab) - 1
        A = self.hx * self.hx
        q = (self.h * A * (T[:, :, k][self.inside] - self.tamb)).sum()
        ins = self.inside
        edge = ins & ~(np.roll(ins, 1, 0) & np.roll(ins, -1, 0) & np.roll(ins, 1, 1) & np.roll(ins, -1, 1))
        top = [kk for kk, n in enumerate(self.lab) if n == "tissue"][-1]
        for kk in range(top + 1, len(self.lab)):
            q += (self.h * self.hx * self.dz[kk] * (T[:, :, kk][edge] - self.tamb)).sum()
        return q


def powers(mode):
    """Average dissipation per part, W. From the pod current budget at 3.8 V [package section 5]. LED electrical power
    minus optical power heats the skin-side D2, most of the optical power heats tissue just below (treated as D2 too)."""
    if mode == "off":
        return {}
    i = {"smart": 1.03e-3, "lite": 0.56e-3, "sleep": 0.46e-3, "full": 1.61e-3}[mode]
    total = i * 3.8
    share = {"D2": 0.42, "U2": 0.22, "U1": 0.22, "U7": 0.04, "L1": 0.02, "U3": 0.03, "U8": 0.01, "U6": 0.02, "U4": 0.002}
    return {k: v * total for k, v in share.items()}


def report(name, m, T, th1=(0.9, 7.0)):
    p = m.probes(T, th1)
    print(f"  {name:<34} skin@U4 {p['skin_under_U4']:.2f}  U4 {p['U4']:.2f}  TH1 {p['TH1']:.2f}  shell {p['shell_top_centre']:.2f}  "
          f"U4-TH1 {p['U4']-p['TH1']:+.3f}  U4-shell {p['U4']-p['shell_top_centre']:+.2f}  skin max/min {p['skin_max_under_pod']:.2f}/{p['skin_min_under_pod']:.2f}  q {1e3*m.flux_through_top(T):.0f} mW")
    return p


def main():
    place, out = sys.argv[1], sys.argv[2]
    R = {}
    print("S6 baseline: 25 C air, h 10 W/m2K")
    m = Model(place, slots=True)
    N = m.nx * m.ny * m.nz
    print(f"  grid {m.nx}x{m.ny}x{m.nz} = {N} voxels", flush=True)
    T0 = m.steady({}); Ton = m.steady(powers("smart")); Tfull = m.steady(powers("full"))
    R["slots_off_power"] = report("slots, electronics off", m, T0)
    R["slots_smart"] = report("slots, smart mode", m, Ton)
    R["slots_full"] = report("slots, full mode", m, Tfull)
    m2 = Model(place, slots=False)
    R["noslot_off"] = report("no slots, electronics off", m2, m2.steady({}))
    R["noslot_smart"] = report("no slots, smart mode", m2, m2.steady(powers("smart")))
    # sensor error vs electronics: slots
    print("  electronics self-heating at U4, slots vs none:",
          round(R["slots_smart"]["U4"] - R["slots_off_power"]["U4"], 4), round(R["noslot_smart"]["U4"] - R["noslot_off"]["U4"], 4))
    print("  LED self-heating sensed at TH1, smart mode:", round(R["slots_smart"]["TH1"] - R["slots_off_power"]["TH1"], 4))

    # Does a sensor pair track heat flux? Sweep the environment: h and ambient change the flux. A true flux sensor has
    # a difference proportional to flux across all conditions. Report slope and scatter.
    print("\nFlux observability: sweep ambient and airflow")
    rows = []
    th1_alt = (5.0, 12.6)
    for h in (5, 10, 20, 40):
        mm = Model(place, slots=False, h_top=h, hx=1e-3)
        for tamb in (15, 20, 25, 30, 34):
            T = mm.steady(powers("smart"), tamb)
            p = mm.probes(T); pa = mm.probes(T, th1_alt)
            rows.append({"tamb": tamb, "h": h, "q_mW": 1e3 * mm.flux_through_top(T), "skin_U4": p["skin_under_U4"], "U4": p["U4"],
                         "TH1": p["TH1"], "TH1alt": pa["TH1"], "shell": p["shell_top_centre"]})
    q = np.array([r["q_mW"] for r in rows])
    for label, d in (("U4 - TH1 (package)", [r["U4"] - r["TH1"] for r in rows]),
                     ("U4 - TH1 at (5.0, 12.6)", [r["U4"] - r["TH1alt"] for r in rows]),
                     ("U4 - top shell sensor", [r["U4"] - r["shell"] for r in rows])):
        d = np.array(d)
        slope, icpt = np.polyfit(q, d, 1)
        resid = d - (slope * q + icpt)
        r2 = 1 - resid.var() / d.var()
        # resolution: TMP117 +/-0.1 C abs, 1% NTC ~ 0.25 C abs. A 100 mW-class signal must beat that.
        print(f"  {label:<26} slope {slope*100:7.4f} K per 100 mW   R2 {r2:.3f}   range {d.min():+.3f} to {d.max():+.3f} K")
        R["flux_" + label] = {"K_per_100mW": slope * 100, "r2": r2, "min": d.min(), "max": d.max()}
    R["sweep"] = rows
    print(f"  flux range in sweep: {q.min():.0f} to {q.max():.0f} mW")

    # Skin reading error vs environment
    e = np.array([r["U4"] - r["skin_U4"] for r in rows])
    print(f"  U4 minus true skin under it: {e.min():+.3f} to {e.max():+.3f} K over the sweep")
    R["u4_skin_err"] = [e.min(), e.max()]
    # how far is covered skin from uncovered skin? The pod itself warms the skin it covers.
    # response time: skin step at t=0 is modelled by electronics step and by donning (pod starts at ambient)
    print("\nTransient: donning a 25 C pod onto skin at steady state")
    dt = 10.0
    m = Model(place, slots=False, h_top=10.0, hx=1e-3)
    Tinit = m.steady({})
    Tstart = Tinit.copy()
    tissue_k = [k for k, n in enumerate(m.lab) if n == "tissue"]
    for k in range(len(m.lab)):
        if k not in tissue_k:
            Tstart[:, :, k] = 25.0
    traj = m.transient(lambda s: powers("smart"), Tstart, dt, 180)
    a, b = m.ix(14.0, 7.0)
    ks = [k for k, n in enumerate(m.lab) if n == "skinparts"][1]
    u4 = np.array([t[a, b, ks] for t in traj])
    final = u4[-1]
    for pct in (0.63, 0.9, 0.99):
        tgt = u4[0] + pct * (final - u4[0])
        idx = np.argmax(u4 >= tgt)
        print(f"  U4 reaches {int(pct*100)} % of its final value after {(idx+1)*dt/60:.1f} min")
        R[f"don_t{int(pct*100)}_min"] = (idx + 1) * dt / 60
    R["u4_final_C"] = final

    # Charging: BQ25101 in the case. Dissipation (V5 - VBAT) * I, skin not present, but check battery and part rise.
    print("\nDock charging, pod in the case (no skin): U5 hot spot")
    for v5, ich in ((5.0, 0.041), (4.6, 0.041)):
        pw = (v5 - 3.7) * ich
        rth_est = None
        print(f"  V5 {v5} V: charger dissipates {pw*1e3:.0f} mW")
        R[f"chg_mW_{v5}"] = pw * 1e3
    json.dump(R, open(out, "w"), default=float, indent=1)


if __name__ == "__main__":
    main()
