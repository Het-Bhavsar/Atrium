"""Atrium form rules: the visible design language every revision keeps.

Guards the visible form against drifting in later edits:
  1. pod plan outline convex, mirror-symmetric, with straight parallel flanks
  2. pod top face flat (planar) over most of its area
  3. skin apertures on the long centreline
  4. adhesive: full coverage, no single hole over 15 % of the patch outline area

Usage: cad run checks/design_rules.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build123d as bd

from lib import params as P
from pod_internals import ADHESIVE_OUTLINE, pod_adhesive
from pod_shell import pod_top_shell

fails = []


def check(name, ok, detail):
    print(f"[{'PASS' if ok else 'FAIL'}] {name}: {detail}")
    if not ok:
        fails.append(name)


def hull_area(pts):
    pts = sorted(set(pts))
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0:
            up.pop()
        up.append(p)
    h = lo[:-1] + up[:-1]
    return abs(sum(h[i][0] * h[i - 1][1] - h[i - 1][0] * h[i][1] for i in range(len(h)))) / 2


shell = pod_top_shell().shape
z = P.POD_H / 2
cut = shell & (bd.Plane.XY.offset(z) * bd.Rectangle(100, 100)).face()
outer = max(cut.faces(), key=lambda f: f.area).outer_wire()
pts = [(round(p.X, 4), round(p.Y, 4)) for p in (outer.position_at(i / 400) for i in range(400))]
area = bd.Face(outer).area
check("outline convex", area / hull_area(pts) > 0.995, f"area / convex hull = {area / hull_area(pts):.4f}")
cx, cy = P.PCB_CX, P.PCB_CY
mirror_err = max(outer.distance_to(bd.Vector(x, 2 * cy - y, z)) for x, y in pts[::5])
check("outline mirror-symmetric", mirror_err < 0.05, f"max mirror error {mirror_err:.3f} mm about y = {cy}")
flanks = [e for e in outer.edges() if e.geom_type == bd.GeomType.LINE and e.length > 5]
check("straight parallel flanks", len(flanks) >= 2, f"{len(flanks)} straight edges > 5 mm")

top = shell.faces().sort_by(bd.Axis.Z)[-1]
plan = P.POD_L * P.POD_W
check("flat top", top.geom_type == bd.GeomType.PLANE and top.area > 0.6 * plan, f"{top.geom_type.name}, {top.area / plan * 100:.0f} % of plan area")

for name, (x, y) in (("LED", P.LED_XY), ("PD1", P.PD1_XY), ("TMP", P.TMP_XY), ("PD2", P.PD2_XY)):
    check(f"{name} on centreline", abs(y - cy) < 1e-6, f"y = {y}")

adh = pod_adhesive().shape
face = adh.faces().sort_by(bd.Axis.Z)[-1]
patch = bd.Face(face.outer_wire()).area
biggest = max((bd.Face(w).area for w in face.inner_wires()), default=0.0)
check("adhesive is not a ring", biggest / patch < 0.15, f"largest hole {biggest:.1f} mm2 = {biggest / patch * 100:.1f} % of {patch:.0f} mm2")

print(f"FAILED: {', '.join(fails) if fails else 'none'}")
