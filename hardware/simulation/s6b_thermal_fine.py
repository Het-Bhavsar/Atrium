"""S6b. Fine-grid thermal comparisons with a solver that converges, replacing the fine-grid block of s6_thermal.py.

The ILU preconditioned CG in s6 did not converge, so its self-heating and slot numbers are not usable. The problem is linear, so here
each load is solved on its own: the base field and each electronics load are separate Jacobi-CG solves, checked by true residual.
Usage (from the repo root):
       python3 hardware/simulation/s6b_thermal_fine.py hardware/pod/pcb/placement/pod_placement.csv hardware/simulation/results/s6b.json
"""
import json, os, sys, time
import numpy as np
import scipy.sparse as sp
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s6_thermal as s6


def pcg(A, b, x0=None, tol=1e-9, maxit=300000):
    """Jacobi conjugate gradient. Returns the solution and the true relative residual."""
    d = A.diagonal()
    x = np.zeros_like(b) if x0 is None else x0.copy()
    r = b - A @ x
    z = r / d
    p = z.copy()
    rz = r @ z
    nb = np.linalg.norm(b)
    for it in range(maxit):
        Ap = A @ p
        a = rz / (p @ Ap)
        x += a * p
        r -= a * Ap
        if it % 200 == 0 and np.linalg.norm(r) < tol * nb:
            break
        z = r / d
        rz2 = r @ z
        p = z + (rz2 / rz) * p
        rz = rz2
    return x, np.linalg.norm(b - A @ x) / nb, it


# Battery current in mA at 3.8 V. Package budget, and the S4 bottom-up figure
LOADS = {"smart, package 1.03 mA": ("smart", 1.0), "full, package 1.61 mA": ("full", 1.0), "smart, bottom-up 0.562 mA": ("smart", 0.562 / 1.03),
         "full, bottom-up 0.958 mA": ("full", 0.958 / 1.61)}


def run(place, slots):
    m = s6.Model(place, slots=slots)
    A = m.A.tocsr()
    t = time.time()
    base, res, it = pcg(A, m.b_core + m.b_amb * 25.0, x0=np.full(A.shape[0], 30.0))
    print(f"  slots={slots}: base field residual {res:.1e} after {it} iterations, {time.time()-t:.0f} s", flush=True)
    out = {"base": m.probes(base.reshape((m.nx, m.ny, m.nz), order="F"))}
    out["base"]["q_mW"] = 1e3 * m.flux_through_top(base.reshape((m.nx, m.ny, m.nz), order="F"))
    out["residual"] = {"base": res}
    for name, (mode, scale) in LOADS.items():
        Q = {k: v * scale for k, v in s6.powers(mode).items()}
        t = time.time()
        d, res, it = pcg(A, m._power(Q))
        D = d.reshape((m.nx, m.ny, m.nz), order="F")
        # the delta field is relative to ambient and core held at zero, so read it directly with the same probes
        p = m.probes(D)
        P = sum(Q.values())
        out[name] = {"power_mW": P * 1e3, "dU4": p["U4"], "dTH1": p["TH1"], "dshell": p["shell_top_centre"], "dskin_U4": p["skin_under_U4"],
                     "d_U4_minus_TH1": p["U4"] - p["TH1"], "d_U4_minus_shell": p["U4"] - p["shell_top_centre"]}
        out["residual"][name] = res
        print(f"  {name:<28} {P*1e3:5.2f} mW  dU4 {p['U4']*1e3:7.1f} mK  dTH1 {p['TH1']*1e3:7.1f} mK  d(U4-TH1) {(p['U4']-p['TH1'])*1e3:+7.1f} mK  dskin {p['skin_under_U4']*1e3:6.1f} mK  residual {res:.1e}  {time.time()-t:.0f} s", flush=True)
    return out


def main():
    R = {}
    for slots in (True, False):
        R["slots" if slots else "no_slots"] = run(sys.argv[1], slots)
    for name in LOADS:
        print(f"  slots benefit at U4, {name}: {R['no_slots'][name]['dU4']*1e3 - R['slots'][name]['dU4']*1e3:+.1f} mK")
    b = R["slots"]["base"]; c = R["no_slots"]["base"]
    print(f"  base, slots: U4 {b['U4']:.2f} skin@U4 {b['skin_under_U4']:.2f} TH1 {b['TH1']:.2f} shell {b['shell_top_centre']:.2f} q {b['q_mW']:.0f} mW; no slots: U4 {c['U4']:.2f} skin@U4 {c['skin_under_U4']:.2f} TH1 {c['TH1']:.2f} q {c['q_mW']:.0f} mW")
    json.dump(R, open(sys.argv[2], "w"), default=float, indent=1)


if __name__ == "__main__":
    main()
