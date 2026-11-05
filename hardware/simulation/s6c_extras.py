"""S6c. Extras read from the S6 sweep and the converged solver: skin estimate from two sensors, TH1 position bias, buck distance after moving PD1.
Usage: python3 s6c_extras.py <pod_placement.csv> <out.json>"""
import csv, json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s6_thermal as s6
from s6b_thermal_fine import pcg
R = {}
rows = json.load(open("results/s6_thermal.json"))["sweep"]
u4 = np.array([r["U4"] for r in rows]); th = np.array([r["TH1"] for r in rows]); sk = np.array([r["skin_U4"] for r in rows])
e0 = u4 - sk
a = np.polyfit(u4 - th, sk - u4, 1)
e1 = u4 + np.polyval(a, u4 - th) - sk
print(f"Skin estimate: U4 alone error {e0.min():+.2f} to {e0.max():+.2f} K. U4 + {a[0]:.2f}*(U4-TH1) {a[1]:+.2f}: {e1.min():+.3f} to {e1.max():+.3f} K, rms {e1.std():.3f} K")
R["skin"] = dict(a=a[0], b=a[1], before=[e0.min(), e0.max()], after=[e1.min(), e1.max()], rms=e1.std())
# sensor absolute errors move the corrected estimate: U4 +/-0.1 K typical band, NTC +/-0.25 K [ASSUMED 1 % NTC]
print(f"  sensor error effect on the corrected estimate: NTC 0.25 K gives {abs(a[0])*0.25:.2f} K, flux error equivalent {0.25/0.9457*100:.0f} mW before offset calibration")
m = s6.Model(sys.argv[1], slots=True)
A = m.A.tocsr()
for name, (mode, scale) in {"smart bottom-up": ("smart", 0.562 / 1.03), "full bottom-up": ("full", 0.958 / 1.61)}.items():
    Q = {k: v * scale for k, v in s6.powers(mode).items()}
    d, res, it = pcg(A, m._power(Q))
    D = d.reshape((m.nx, m.ny, m.nz), order="F")
    p1 = m.probes(D, (0.9, 7.0)); p2 = m.probes(D, (5.0, 12.6))
    print(f"  {name}: TH1 at (0.9,7.0) d(U4-TH1) {(p1['U4']-p1['TH1'])*1e3:+.1f} mK; TH1 at (5.0,12.6) {(p2['U4']-p2['TH1'])*1e3:+.1f} mK; residual {res:.1e}")
    R[name] = dict(th1=(p1["U4"] - p1["TH1"]), alt=(p2["U4"] - p2["TH1"]))
P = {r["ref"]: r for r in csv.DictReader(open(sys.argv[1]))}
def gap(a, b):
    ax, ay, aw, ah = [float(a[k]) for k in ("x_mm", "y_mm", "w_mm", "h_mm")]
    bx, by, bw, bh = [float(b[k]) for k in ("x_mm", "y_mm", "w_mm", "h_mm")]
    dx = max(abs(ax - bx) - (aw + bw) / 2, 0); dy = max(abs(ay - by) - (ah + bh) / 2, 0)
    return (dx * dx + dy * dy) ** 0.5
for pdx in (8.2, 5.7):
    pd = dict(P["PD1"]); pd["x_mm"] = pdx
    print(f"  PD1 at x={pdx}: " + ", ".join(f"{r} {gap(P[r], pd):.2f} mm ({P[r]['side']} at {P[r]['x_mm']},{P[r]['y_mm']})" for r in ("U7", "L1", "U2")))
print("  D2 edge to PD1 edge at x=5.7:", round(gap(P["D2"], dict(P["PD1"], x_mm=5.7)), 2), "mm; PD1 edge to U4 edge", round(gap(P["U4"], dict(P["PD1"], x_mm=5.7)), 2), "mm")
json.dump(R, open(sys.argv[2], "w"), indent=1, default=float)
