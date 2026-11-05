"""Atrium charging case fit checks: pogo engagement, USB-C vs docked pod, case cell
swell, lid magnet field at the hall sensor. Writes renders/case_fit_report.txt.

Usage: cad run checks/fit.py
"""

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE.parents[1]))
sys.path.insert(0, str(HERE.parents[4] / "pod" / "mechanical" / "src"))

from cad.geometry import overlap_volume

import case as C
from case_assembly import case_assembly
from checks.interference import world_parts
from lib import params as P
from lib.geom import case_parts, part_row

OUT = HERE.parents[2] / "renders" / "case_fit_report.txt"
lines = []
fails = []


def say(s=""):
    print(s)
    lines.append(s)


def check(name, ok, detail):
    tag = "PASS" if ok else "FAIL"
    if not ok:
        fails.append(name)
    say(f"[{tag}] {name}: {detail}")


say("ATRIUM CASE FIT CHECK")
say("=" * 64)

say("Case:")
plunger_reach = P.POGO_WORK_H - P.POGO_BARREL_H
check(
    "pogo engagement",
    plunger_reach >= P.POD_PAD_DEPTH - (P.Z_CRADLE_FLOOR - P.Z_CASE_PCB_TOP - P.POGO_BARREL_H),
    f"cradle floor {P.Z_CRADLE_FLOOR:.2f}, barrel top {P.Z_CASE_PCB_TOP + P.POGO_BARREL_H:.2f}, "
    f"plunger tip {P.Z_CASE_PCB_TOP + P.POGO_WORK_H:.2f} = pad face {P.Z_CRADLE_FLOOR + P.POD_PAD_DEPTH:.2f}",
)
check(
    "pogo stroke",
    P.POGO_WORK_COMP <= 0.7 * P.POGO_STROKE_MAX,
    f"working compression {P.POGO_WORK_COMP} vs max stroke {P.POGO_STROKE_MAX} [VERIFY part drawing]",
)
hole_clear = (P.POGO_HOLE_D - P.POGO_PLUNGER_D) / 2
check("plunger in window hole", hole_clear >= P.CRADLE_POCKET_CLEAR + 0.05, f"radial {hole_clear:.2f} vs pocket float {P.CRADLE_POCKET_CLEAR}")

case = case_assembly().shape
j1 = [p for p in world_parts(case) if p.label == "J1"][0]
ov = 0.0
for p in world_parts(case):
    if p.label in ("pod_top_shell", "window_frame_black_pc"):
        ov += overlap_volume(j1, p)
j1r = part_row(case_parts(), "J1")
pod_ymin = P.POD_DOCK_OFFSET[1] + P.PCB_CY - P.POD_W / 2
shift = j1r["y"] + j1r["h"] / 2 + 0.3 - pod_ymin
lift = P.Z_CASE_PCB_TOP + j1r["z"] + 0.3 - P.Z_CRADLE_FLOOR
check(
    "USB-C J1 vs docked pod",
    ov < 1e-4,
    f"overlap {ov:.2f} mm3: J1 rear y {j1r['y'] + j1r['h'] / 2:.1f}, top {P.Z_CASE_PCB_TOP + j1r['z']:.2f}; "
    f"pod skin y {pod_ymin:.1f}, z {P.Z_CRADLE_FLOOR:.2f}. Needs pod/pins +{shift:.1f} mm in y, or J1 {lift:.1f} mm lower",
)
cell_t, cell_tmax = P.CASE_CELL[2], P.CASE_CELL_MAX[2]
foam_solid = P.CASE_CELL_FOAM_T * P.PORON_SOLID_FRAC
res = P.CASE_CELL_POCKET_H - cell_tmax * (1 + P.CELL_SWELL) - foam_solid
check("case cell swell (6.3 max +10 %)", res >= 0, f"pocket {P.CASE_CELL_POCKET_H} - {cell_tmax * 1.1:.2f} - foam solid {foam_solid:.2f} = {res:+.2f}")
lw = P.CASE_CELL_MAX
check(
    "case cell footprint (max)",
    P.CASE_CELL_X0 + lw[0] <= C.X1 and lw[1] <= C.Y1 - C.Y0,
    f"x {P.CASE_CELL_X0:.1f}-{P.CASE_CELL_X0 + lw[0]:.1f} in {C.X0:.1f}-{C.X1:.1f}, w {lw[1]} in {C.Y1 - C.Y0:.1f}",
)

die_z = P.Z_CASE_PCB_TOP + P.HALL_DIE_Z
d_closed = C.Z_MAGNET_BOT - die_z
b_closed = C.magnet_field(d_closed)
check("hall closed", b_closed >= 2 * P.HALL_BOP_MAX, f"magnet face {d_closed:.2f} mm above die: {b_closed * 1e3:.0f} mT vs BOP max {P.HALL_BOP_MAX * 1e3:.1f} mT x2")
mx, my = P.MAGNET_XY
ang = math.radians(100)
rx, rz = mx - C.HINGE_X, C.Z_MAGNET_BOT - C.HINGE_Z
ox = C.HINGE_X + rx * math.cos(ang) - rz * math.sin(ang)
oz = C.HINGE_Z + rx * math.sin(ang) + rz * math.cos(ang)
d_open = math.hypot(ox - mx, oz - die_z)
b_open = C.magnet_field(d_open)
check("hall open (lid 100 deg)", b_open <= P.HALL_BRP_MIN, f"{d_open:.1f} mm: {b_open * 1e3:.3f} mT vs BRP min {P.HALL_BRP_MIN * 1e3:.1f} mT (on-axis bound)")
for ang_deg in (10, 20, 30):
    a = math.radians(ang_deg)
    px = C.HINGE_X + rx * math.cos(a) - rz * math.sin(a)
    pz = C.HINGE_Z + rx * math.sin(a) + rz * math.cos(a)
    say(f"    lid {ang_deg:2d} deg: {math.hypot(px - mx, pz - die_z):5.1f} mm, {C.magnet_field(math.hypot(px - mx, pz - die_z)) * 1e3:6.2f} mT")
say()
from case import case_base, case_cradle, case_lid  # noqa: E402

import build123d as bd  # noqa: E402

base_s, cradle_s, lid_s = case_base().shape, case_cradle().shape, case_lid().shape
pod_s = [p for p in world_parts(case) if p.label == "pod_top_shell"][0]
axis = bd.Axis((C.HINGE_X, 0, C.HINGE_Z), (0, 1, 0))
worst = 0.0
for ang in (10, 30, 60, 90, 110):
    moved = lid_s.rotate(axis, -ang)
    ov = sum(overlap_volume(moved, other) for other in (base_s, cradle_s, pod_s))
    worst = max(worst, ov)
    say(f"    lid at {ang:3d} deg: overlap {ov:.4f} mm3")
check("lid swing 10-110 deg", worst < 1e-3, f"max overlap {worst:.4f} mm3 with base, cradle and pod")
say()
say(f"Case outer: {C.OX1 - C.OX0:.1f} x {C.OY1 - C.OY0:.1f} x {P.Z_LID_TOP:.1f} mm (hinge knuckles add {C.OX0 - (C.HINGE_X - C.KNUCKLE_D / 2):.1f} mm)")
say()
say(f"FAILED: {', '.join(fails) if fails else 'none'}")
OUT.parent.mkdir(exist_ok=True)
OUT.write_text("\n".join(lines) + "\n")
