"""S7. 3D FDTD of the pod antenna region at 2.44 GHz, in free space and on a layered head.

This is a comparative model. The Walsin part's internals aren't public, so the chip is a stand-in: a short strip in a
3.1 x 1.6 x 0.6 mm ceramic block on the real PCB ground and cell geometry. At this ground size radiation comes mostly from
ground and cell currents, so body loss and detuning trends carry over; absolute gain does not. Copper loss is not modelled.

Tissue at 2.45 GHz, Gabriel 1996 values: skin 38.0/1.46, fat 5.28/0.105, muscle 52.7/1.74, cortical bone 11.4/0.394,
grey matter 48.9/1.81 (eps_r / S/m).

Usage: python3 -I s7_rf_fdtd.py <case|all|dipole> <out.json>
"""
import json, math, sys, time
import numpy as np
from numba import njit, prange
from scipy import ndimage

C0 = 299792458.0; EPS0 = 8.8541878128e-12; MU0 = 4e-7 * np.pi
D = 0.5e-3
F0 = 2.44e9
NABS = 24


@njit(parallel=True, cache=True)
def step_h(Ex, Ey, Ez, Hx, Hy, Hz, Da, Db):
    nx, ny, nz = Ex.shape
    for i in prange(nx - 1):
        for j in range(ny - 1):
            for k in range(nz - 1):
                a = Da[i, j, k]; b = Db[i, j, k]
                Hx[i, j, k] = a * Hx[i, j, k] + b * ((Ey[i, j, k + 1] - Ey[i, j, k]) - (Ez[i, j + 1, k] - Ez[i, j, k]))
                Hy[i, j, k] = a * Hy[i, j, k] + b * ((Ez[i + 1, j, k] - Ez[i, j, k]) - (Ex[i, j, k + 1] - Ex[i, j, k]))
                Hz[i, j, k] = a * Hz[i, j, k] + b * ((Ex[i, j + 1, k] - Ex[i, j, k]) - (Ey[i + 1, j, k] - Ey[i, j, k]))


@njit(parallel=True, cache=True)
def step_e(Ex, Ey, Ez, Hx, Hy, Hz, Cax, Cbx, Cay, Cby, Caz, Cbz):
    nx, ny, nz = Ex.shape
    for i in prange(1, nx - 1):
        for j in range(1, ny - 1):
            for k in range(1, nz - 1):
                Ex[i, j, k] = Cax[i, j, k] * Ex[i, j, k] + Cbx[i, j, k] * ((Hz[i, j, k] - Hz[i, j - 1, k]) - (Hy[i, j, k] - Hy[i, j, k - 1]))
                Ey[i, j, k] = Cay[i, j, k] * Ey[i, j, k] + Cby[i, j, k] * ((Hx[i, j, k] - Hx[i, j, k - 1]) - (Hz[i, j, k] - Hz[i - 1, j, k]))
                Ez[i, j, k] = Caz[i, j, k] * Ez[i, j, k] + Cbz[i, j, k] * ((Hy[i, j, k] - Hy[i - 1, j, k]) - (Hx[i, j, k] - Hx[i, j - 1, k]))


@njit(parallel=True, cache=True)
def dft_accum(Ex, Ey, Ez, Rx, Ix, Ry, Iy, Rz, Iz, c, s, kmax):
    nx, ny, _ = Ex.shape
    for i in prange(nx):
        for j in range(ny):
            for k in range(kmax):
                Rx[i, j, k] += Ex[i, j, k] * c; Ix[i, j, k] -= Ex[i, j, k] * s
                Ry[i, j, k] += Ey[i, j, k] * c; Iy[i, j, k] -= Ey[i, j, k] * s
                Rz[i, j, k] += Ez[i, j, k] * c; Iz[i, j, k] -= Ez[i, j, k] * s


def stadium_mask(X, Y, L=28.0, W=14.0):
    """X, Y in PCB coordinates mm."""
    R = W / 2
    m = (X >= R) & (X <= L - R) & (Y >= 0) & (Y <= W)
    m |= (X - R) ** 2 + (Y - R) ** 2 <= R ** 2
    m |= (X - (L - R)) ** 2 + (Y - R) ** 2 <= R ** 2
    return m


def build_pod(case):
    """Returns eps, sigma per cell, pec masks and the port. Coordinates: PCB lower-left at pad, skin plane at z=zskin."""
    margin = 20.0
    head = case.get("head", True)
    tissue_mm = 30.0 if head else 0.0
    pod_h = 7.05
    Lx, Ly = 30.0 + 2 * margin, 16.0 + 2 * margin
    Lz = tissue_mm + pod_h + 2 * margin + (0.0 if head else margin / 2)
    pad = NABS * D * 1e3
    nx = int((Lx + 2 * pad) / (D * 1e3)); ny = int((Ly + 2 * pad) / (D * 1e3)); nz = int((Lz + 2 * pad) / (D * 1e3))
    x0 = pad + margin + 1.0; y0 = pad + margin + 1.0
    zskin = pad + (tissue_mm if head else margin)
    xc = (np.arange(nx) + 0.5) * D * 1e3 - x0
    yc = (np.arange(ny) + 0.5) * D * 1e3 - y0
    zc = (np.arange(nz) + 0.5) * D * 1e3 - zskin
    X, Y = np.meshgrid(xc, yc, indexing="ij")
    eps = np.ones((nx, ny, nz), np.float32); sig = np.zeros((nx, ny, nz), np.float32)
    tag = np.zeros((nx, ny, nz), np.int8)   # 1 tissue, 2 FR4, 3 plastic, 4 ceramic
    if head:
        depths = [(0.0, 1.5, 38.0, 1.46), (1.5, 3.0, 5.28, 0.105), (3.0, 10.0, 52.7, 1.74), (10.0, 13.0, 11.4, 0.394), (13.0, 200.0, 48.9, 1.81)]
        for z0, z1, e, s in depths:
            m = (zc <= -z0) & (zc > -z1)
            eps[:, :, m] = e; sig[:, :, m] = s; tag[:, :, m] = 1
    pod = stadium_mask(X + 1.0, Y + 1.0, 30.0, 16.0)
    pcb = stadium_mask(X, Y)
    # shell: PC walls and lid, foam and air inside; window and adhesive under the parts
    pc_e, pc_s = 2.8, 2 * np.pi * F0 * EPS0 * 2.8 * 0.01
    zw0, zw1 = 0.25, 0.75
    ks = lambda a, b: np.where((zc >= a) & (zc < b))[0]
    for k in ks(zw0, zw1):
        eps[:, :, k][pod] = pc_e; sig[:, :, k][pod] = pc_s; tag[:, :, k][pod] = 3
    for k in ks(0.0, 0.25):
        eps[:, :, k][pod] = 3.0; sig[:, :, k][pod] = pc_s; tag[:, :, k][pod] = 3
    wall = pod & ~pcb
    for k in ks(0.75, 6.55):
        eps[:, :, k][wall] = pc_e; sig[:, :, k][wall] = pc_s; tag[:, :, k][wall] = 3
    for k in ks(6.55, 7.05):
        eps[:, :, k][pod] = pc_e; sig[:, :, k][pod] = pc_s; tag[:, :, k][pod] = 3
    # PCB dielectric 0.6 mm: 1.75 to 2.35
    fr4_e, fr4_s = 4.3, 2 * np.pi * F0 * EPS0 * 4.3 * 0.02
    kp = ks(1.75, 2.35)
    for k in kp:
        eps[:, :, k][pcb] = fr4_e; sig[:, :, k][pcb] = fr4_s; tag[:, :, k][pcb] = 2
    kg = kp[len(kp) // 2]                        # ground sheet, cell boundary at the middle of the board
    gx = case.get("ground_x", 24.0)
    ground = pcb & (X < gx)
    pec_xy = np.zeros((nx, ny, nz), bool)
    pec_xy[:, :, kg][ground] = True
    pec_box = np.zeros((nx, ny, nz), bool)
    # cell: conductive box, 3.25 to 6.25 above skin, x from 3 to cell_x
    cx = case.get("cell_x", 23.0)
    cell = (X >= 3.0) & (X <= cx) & (Y >= 2.0) & (Y <= 12.0)
    for k in ks(3.25, 6.25):
        pec_box[:, :, k][cell] = True
    # nickel tabs tie the cell to the board at the low-x end
    tab = (X >= 3.0) & (X <= 4.0) & (np.abs(Y - 4.4) <= 0.5)
    for k in range(kg, ks(3.25, 3.75)[0] + 1):
        pec_box[:, :, k][tab] = True
    if case.get("cell_tied"):
        tab2 = (X >= cx - 1.5) & (X <= cx - 0.5) & (np.abs(Y - 7.0) <= 3.0)
        for k in range(kg, ks(3.25, 3.75)[0] + 1):
            pec_box[:, :, k][tab2] = True
    # chip antenna stand-in: feed gap at ground edge, strip to the block, arm inside a ceramic block
    ix = lambda v: int(np.argmin(abs(xc - v)))
    jy = lambda v: int(np.argmin(abs(yc - v)))
    is_ = ix(gx + 0.25)
    jf = jy(7.0)
    lift = case.get("ant_lift", 0.0)
    ka = kg + 1 + int(round(lift / (D * 1e3)))
    kt = ka + 1
    # ceramic block on top of the keep-out
    bx0, bx1, by0, by1 = 25.2, 26.8, 5.45, 8.55
    for k in range(ka, kt + 1):
        m = (X >= bx0) & (X <= bx1) & (Y >= by0) & (Y <= by1)
        eps[:, :, k][m] = 20.0; tag[:, :, k][m] = 4
    if lift > 0:                                # a raised antenna sits on plastic instead of air
        for k in range(kg + 1, ka):
            m = (X >= bx0 - 0.5) & (X <= bx1 + 0.5) & (Y >= by0 - 0.5) & (Y <= by1 + 0.5)
            eps[:, :, k][m] = pc_e; sig[:, :, k][m] = pc_s; tag[:, :, k][m] = 3
    strip = np.zeros((nx, ny, nz), bool)
    i_end = ix(26.0)
    strip[is_ + 1:i_end + 1, jf, kg] = True      # feed line in the ground plane level
    for k in range(kg, ka + 1):
        strip[i_end, jf, k] = True               # riser
    # arm along +y in the block, then folded -y, about 6 mm total
    j1 = jy(8.3); j0 = jy(5.7)
    strip[i_end, jf:j1 + 1, ka] = True
    strip[i_end - 1 : i_end + 1, j1, ka] = True
    strip[i_end - 1, j0:j1 + 1, ka] = True
    port = (is_, jf, kg)
    return dict(eps=eps, sig=sig, tag=tag, pec_xy=pec_xy, pec_box=pec_box, strip=strip, port=port, kg=kg, kmax=ks(7.05, 7.5)[0],
                zc=zc, shape=(nx, ny, nz), name=case["name"])


def build_dipole(length_mm=56.0):
    """Validation: centre-fed thin PEC wire along x in free space."""
    margin = 30.0
    Lx = length_mm + 2 * margin; Ly = 2 * margin; Lz = 2 * margin
    pad = NABS * D * 1e3
    nx = int((Lx + 2 * pad) / (D * 1e3)); ny = int((Ly + 2 * pad) / (D * 1e3)); nz = int((Lz + 2 * pad) / (D * 1e3))
    eps = np.ones((nx, ny, nz), np.float32); sig = np.zeros((nx, ny, nz), np.float32)
    tag = np.zeros((nx, ny, nz), np.int8)
    strip = np.zeros((nx, ny, nz), bool)
    ic = nx // 2; jc = ny // 2; kc = nz // 2
    half = int(length_mm / 2 / (D * 1e3))
    strip[ic - half:ic + half + 1, jc, kc] = True   # the port edge at ic is reopened as the source in simulate()
    return dict(eps=eps, sig=sig, tag=tag, pec_xy=np.zeros_like(strip), pec_box=np.zeros_like(strip), strip=strip, port=(ic, jc, kc), kg=kc, kmax=2,
                zc=np.zeros(nz), shape=(nx, ny, nz), name="dipole")


def coeffs(g, dt):
    eps, sig = g["eps"], g["sig"]
    nx, ny, nz = g["shape"]
    # average onto edges
    def avg(a, ax1, ax2):
        return 0.25 * (a + np.roll(a, 1, ax1) + np.roll(a, 1, ax2) + np.roll(np.roll(a, 1, ax1), 1, ax2))
    # graded absorber: conductivity profile added to E, matched magnetic loss on H
    prof = np.zeros((nx, ny, nz), np.float32)
    sm = 0.8 * 4 / (377.0 * D)
    def grade(n):
        p = np.zeros(n, np.float32)
        for i in range(NABS):
            v = ((NABS - i) / NABS) ** 3
            p[i] = v; p[n - 1 - i] = v
        return p
    px, py, pz = grade(nx), grade(ny), grade(nz)
    prof = np.maximum(np.maximum(px[:, None, None], py[None, :, None]), pz[None, None, :]) * sm
    sig_e = sig + prof.astype(np.float32)
    out = {}
    for name, (a1, a2) in {"x": (1, 2), "y": (0, 2), "z": (0, 1)}.items():
        e = avg(eps, a1, a2); s = avg(sig_e, a1, a2)
        den = 1 + s * dt / (2 * EPS0 * e)
        out["Ca" + name] = ((1 - s * dt / (2 * EPS0 * e)) / den).astype(np.float32)
        out["Cb" + name] = ((dt / (EPS0 * e * D)) / den).astype(np.float32)
    s_m = prof * MU0 / EPS0
    den = 1 + s_m * dt / (2 * MU0)
    out["Da"] = ((1 - s_m * dt / (2 * MU0)) / den).astype(np.float32)
    out["Db"] = ((dt / (MU0 * D)) / den).astype(np.float32)
    # PEC: kill E tangential on the metal
    gxy, box, strip = g["pec_xy"], g["pec_box"], g["strip"]
    for name in ("x", "y"):
        out["Ca" + name][gxy] = 0; out["Cb" + name][gxy] = 0
    # A solid box zeroes all three components in its cells
    for name in ("x", "y", "z"):
        out["Ca" + name][box] = 0; out["Cb" + name][box] = 0
    # A wire or strip cell zeroes edges along +axis to the next strip cell
    for name, ax in (("x", 0), ("y", 1), ("z", 2)):
        m = strip & np.roll(strip, -1, ax)
        out["Ca" + name][m] = 0; out["Cb" + name][m] = 0
    # the port edge must connect to metal on both sides. Close the ground side: edge from the sheet to the gap is sheet.
    return out


def simulate(g, steps=10000, r_s=50.0, verbose=True):
    dt = 0.99 * D / (C0 * math.sqrt(3))
    co = coeffs(g, dt)
    nx, ny, nz = g["shape"]
    Ex = np.zeros((nx, ny, nz), np.float32); Ey = Ex.copy(); Ez = Ex.copy()
    Hx = Ex.copy(); Hy = Ex.copy(); Hz = Ex.copy()
    ip, jp, kp = g["port"]
    a = dt / (2 * r_s * EPS0 * D)
    # resistive source on the Ex edge
    co["Cax"][ip, jp, kp] = (1 - a) / (1 + a)
    co["Cbx"][ip, jp, kp] = (dt / (EPS0 * D)) / (1 + a)
    src = (dt / (r_s * EPS0 * D * D)) / (1 + a)
    tau = 0.35e-9; t0 = 1.1e-9
    V = np.zeros(steps); I = np.zeros(steps)
    kmax = min(g["kmax"], nz)
    Rx = np.zeros((nx, ny, kmax), np.float32); Ix = Rx.copy(); Ry = Rx.copy(); Iy = Rx.copy(); Rz = Rx.copy(); Iz = Rx.copy()
    w = 2 * np.pi * F0
    every = 8
    t0_wall = time.time()
    peak = 0.0
    for n in range(steps):
        step_h(Ex, Ey, Ez, Hx, Hy, Hz, co["Da"], co["Db"])
        I[n] = ((Hz[ip, jp, kp] - Hz[ip, jp - 1, kp]) - (Hy[ip, jp, kp] - Hy[ip, jp, kp - 1])) * D
        step_e(Ex, Ey, Ez, Hx, Hy, Hz, co["Cax"], co["Cbx"], co["Cay"], co["Cby"], co["Caz"], co["Cbz"])
        t = (n + 0.5) * dt
        vs = math.exp(-((t - t0) / tau) ** 2) * math.sin(2 * np.pi * F0 * (t - t0))
        Ex[ip, jp, kp] -= src * vs
        V[n] = -Ex[ip, jp, kp] * D
        if n % every == 0 and kmax > 2:
            tn = (n + 1) * dt
            dft_accum(Ex, Ey, Ez, Rx, Ix, Ry, Iy, Rz, Iz, math.cos(w * tn) * every * dt, math.sin(w * tn) * every * dt, kmax)
        peak = max(peak, abs(V[n]))
        if verbose and n and n % 4000 == 0:
            print(f"   step {n}/{steps}  {time.time()-t0_wall:.0f}s  |V| now/peak {abs(V[n])/peak:.1e}", flush=True)
    tn = (np.arange(steps) + 1) * dt
    tI = (np.arange(steps) + 0.5) * dt
    freqs = np.arange(1.8e9, 3.2e9, 0.02e9)
    Vf = np.array([(V * np.exp(-2j * np.pi * f * tn)).sum() * dt for f in freqs])
    If = np.array([(I * np.exp(-2j * np.pi * f * tI)).sum() * dt for f in freqs])
    Z = Vf / If
    i0 = int(np.argmin(abs(freqs - F0)))
    P_in = 0.5 * (Vf[i0] * np.conj(If[i0])).real
    sig = g["sig"][:, :, :kmax]; tag = g["tag"][:, :, :kmax]
    E2 = (Rx ** 2 + Ix ** 2 + Ry ** 2 + Iy ** 2 + Rz ** 2 + Iz ** 2).astype(np.float64)
    dV = D ** 3
    pl = 0.5 * sig * E2 * dV
    P_t = pl[tag == 1].sum(); P_f = pl[tag == 2].sum(); P_s = pl[tag == 3].sum()
    # Peak local 1 g SAR at 1 mW accepted
    sar = None
    if (tag == 1).any():
        rho = 1050.0
        pv = np.where(tag == 1, pl / dV / rho, 0.0)
        n1g = int(round((1e-6 / D ** 3 * (1000.0 / rho)) ** (1 / 3) / 1)) if False else 20
        avg = ndimage.uniform_filter(pv, size=n1g, mode="constant")
        sar = float(avg.max() / P_in * 1e-3)
    decay = float(np.abs(V[-200:]).max() / peak)
    return dict(name=g["name"], freqs=freqs.tolist(), Z_re=Z.real.tolist(), Z_im=Z.imag.tolist(), P_in=float(P_in), f_tissue=float(P_t / P_in),
                f_fr4=float(P_f / P_in), f_plastic=float(P_s / P_in), eta=float(1 - (P_t + P_f + P_s) / P_in), sar_1g_per_mW=sar, decay=decay, wall=time.time() - t0_wall, cells=int(nx * ny * nz))


CASES = {
    "free": dict(name="free space", head=False),
    "head": dict(name="on head, as designed"),
    "head_cell21": dict(name="on head, cell ends at x=21", cell_x=21.0),
    "head_tied": dict(name="on head, cell tied to ground at the antenna end", cell_tied=True),
    "head_lift": dict(name="on head, antenna raised 2.5 mm into the shell", ant_lift=2.5),
    "free_tied": dict(name="free space, cell tied to ground", head=False, cell_tied=True),
}


def main():
    which = sys.argv[1]; out = sys.argv[2]
    R = {}
    names = list(CASES) if which == "all" else [which]
    if which == "dipole":
        g = build_dipole()
        r = simulate(g, steps=5000)
        Z = np.array(r["Z_re"]) + 1j * np.array(r["Z_im"])
        f = np.array(r["freqs"])
        fres = f[np.argmin(abs(Z.imag))]
        print(f"dipole 56 mm: resonance {fres/1e9:.2f} GHz (0.47 lambda rule gives {0.47*C0/56e-3/1e9:.2f}), R there {Z.real[np.argmin(abs(Z.imag))]:.1f} ohm, decay {r['decay']:.1e}")
        R["dipole"] = r
    else:
        for n in names:
            print(f"case {n}: {CASES[n]['name']}", flush=True)
            g = build_pod(CASES[n])
            r = simulate(g)
            R[n] = r
            i0 = int(np.argmin(abs(np.array(r["freqs"]) - F0)))
            print(f"  Z(2.44) {r['Z_re'][i0]:.2f} {r['Z_im'][i0]:+.2f}j  tissue {100*r['f_tissue']:.1f}%  FR4 {100*r['f_fr4']:.1f}%  plastic {100*r['f_plastic']:.1f}%  radiated {100*r['eta']:.1f}% ({10*math.log10(max(r['eta'],1e-4)):.1f} dB)  SAR1g/mW {r['sar_1g_per_mW']}  decay {r['decay']:.1e}  wall {r['wall']:.0f}s", flush=True)
            json.dump(R, open(out, "w"))
    json.dump(R, open(out, "w"))


if __name__ == "__main__":
    main()
