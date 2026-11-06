"""Atrium stack-up, tolerance and fit checks. Prints a report and writes
renders/stackup_report.txt.

Pod z-stack: worst-case and RSS of the gap above the cell, then a 10 % cell swell
against the PORON's solid height, for the nominal cell and the supplier-sheet max.
Also: skin-side clearances, thermal gap pad compression, antenna lift room, metal at
x >= 24. Case fits are in hardware/case/mechanical/src/checks/fit.py.

Usage: cad run checks/stackup.py
"""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from checks.interference import world_parts
from lib import params as P
from lib.geom import pod_parts
from pod_assembly import pod_assembly
from pod_window import rib_led_pd1_span

OUT = Path(__file__).resolve().parents[2] / "renders" / "stackup_report.txt"
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


# ------------------------------------------------------------------ pod z-stack
rows = pod_parts()
cell_x0, cell_y0 = P.CELL_X0, P.CELL_Y0


def tallest_top_under(x0, y0, x1, y1):
    best = (0.0, None)
    for r in rows:
        if r["side"] != "top" or r["ref"] == "AE1":
            continue
        if r["x"] + r["w"] / 2 <= x0 or r["x"] - r["w"] / 2 >= x1:
            continue
        if r["y"] + r["h"] / 2 <= y0 or r["y"] - r["h"] / 2 >= y1:
            continue
        if r["z"] > best[0]:
            best = (r["z"], r["ref"])
    return best


top_nom_h, top_ref = tallest_top_under(cell_x0, cell_y0, cell_x0 + P.CELL_L, cell_y0 + P.CELL_W)
top_max_h, top_max_ref = tallest_top_under(
    cell_x0 - 0.25, P.PCB_CY - P.CELL_W_SHEET_MAX / 2, cell_x0 + P.CELL_L_SHEET_MAX, P.PCB_CY + P.CELL_W_SHEET_MAX / 2
)

# (name, nominal, +/- tolerance, sign: +1 adds height to the stack, -1 is the envelope)
TOL = [
    ("outer height", P.POD_H, 0.05, -1),
    ("top shell", P.SHELL_TOP_T, 0.05, +1),
    ("window plate", P.WINDOW_T, 0.03, +1),
    ("rib / rim height", P.SKIN_ZONE, 0.03, +1),
    (f"PCB {P.PCB_T} (JLC +/-{P.PCB_T_TOL})", P.PCB_T, P.PCB_T_TOL, +1),
    (f"parts under cell (rule)", P.UNDER_CELL_MAX, 0.0, +1),
    ("PI tape", P.CELL_INSULATOR_T, 0.01, +1),
]

say("ATRIUM REV B.1 MECHANICAL STACK-UP CHECK")
say("=" * 64)
say("Pod z-stack (skin face z = 0), mm:")
z = 0.0
for name, nom, tol, sign in TOL[1:]:
    say(f"  {name:28s} {nom:5.2f}  +/-{tol:.2f}")
say(f"  {'cell (nominal)':28s} {P.CELL_T:5.2f}  +/-{P.CELL_T_TOL:.2f}  [ASSUMED tol]")
gap_nom = P.POD_H - sum(n for _, n, _, s in TOL if s > 0) - P.CELL_T
say(f"  {'gap above cell (PORON)':28s} {gap_nom:5.2f}")
say(f"  {'outer height':28s} {P.POD_H:5.2f}")
say(f"  tallest top part under the cell: {top_ref} {top_nom_h:.2f} (layout rule {P.UNDER_CELL_MAX})")
say()

poron_solid = P.PORON_T * P.PORON_SOLID_FRAC
tol_list = [t for _, _, t, _ in TOL] + [P.CELL_T_TOL]
wc = sum(tol_list)
rss = math.sqrt(sum(t * t for t in tol_list))


def swell_case(label, cell_t, gap_reduction, using_actual_parts):
    part_credit = (P.UNDER_CELL_MAX - top_nom_h) if using_actual_parts else 0.0
    gap = P.POD_H - sum(n for _, n, _, s in TOL if s > 0) - cell_t - gap_reduction + part_credit
    swell = P.CELL_SWELL * cell_t
    residual = gap - swell - poron_solid
    comp = 1 - (gap - swell) / P.PORON_T if gap - swell < P.PORON_T else 0.0
    check(
        f"swell {label}",
        residual >= 0,
        f"gap {gap:.3f} - swell {swell:.3f} - PORON solid {poron_solid:.2f} = {residual:+.3f} mm"
        + (f" (PORON at {comp * 100:.0f} % compression)" if residual >= 0 else ""),
    )
    return residual


say(f"Cell swell 10 % vs gap above the cell (PORON free {P.PORON_T:.2f}, solid {poron_solid:.2f}):")
check("layout rule: parts under the cell", top_nom_h <= P.UNDER_CELL_MAX, f"tallest {top_ref} {top_nom_h} vs rule {P.UNDER_CELL_MAX}")
swell_case(f"nominal 3.0, rule {P.UNDER_CELL_MAX}", P.CELL_T, 0.0, False)
swell_case("RSS tolerances", P.CELL_T, rss - 0.0, False)
swell_case("worst-case tolerances", P.CELL_T, wc, False)
swell_case("sheet max 3.5", P.CELL_T_SHEET_MAX, 0.0, False)
need = P.CELL_T * (1 + P.CELL_SWELL) + poron_solid + wc - (P.POD_H - sum(n for _, n, _, s in TOL if s > 0))
say(f"  -> worst-case pass needs outer height >= {P.POD_H + max(need, 0):.2f} mm (3.0 cell)")
need_max = P.CELL_T_SHEET_MAX * (1 + P.CELL_SWELL) + poron_solid - (P.POD_H - sum(n for _, n, _, s in TOL if s > 0))
say(f"  -> sheet-max cell needs outer height >= {P.POD_H + need_max:.2f} mm (nominal tolerances)")
check(
    "cell footprint vs battery zone",
    P.CELL_L_SHEET_MAX <= 20.0 and P.CELL_W_SHEET_MAX <= 10.0,
    f"sheet max {P.CELL_L_SHEET_MAX} x {P.CELL_W_SHEET_MAX} vs zone 20 x 10; parts under the larger footprint "
    f"tallest {top_max_ref} {top_max_h:.2f} (<= {P.TOP_ZONE}), end at x {cell_x0 + P.CELL_L_SHEET_MAX:.1f} (< 24)",
)
say()

# ------------------------------------------------------------------ PCB thickness extremes
say(f"PCB thickness {P.PCB_T_MIN:.2f} / {P.PCB_T:.2f} / {P.PCB_T_MAX:.2f} mm (rib datum fixes the skin face):")
for t in (P.PCB_T_MIN, P.PCB_T, P.PCB_T_MAX):
    top = P.Z_PCB_BOT + t
    pad = P.Z_TAB_BOT - top
    comp = 1 - pad / P.TAB_PAD_T
    check(f"PCB {t:.2f}: tab pad", 0.0 <= comp <= 0.70, f"space {pad:.2f} for {P.TAB_PAD_T} pad -> {comp * 100:.0f} % compression")
    gap = P.Z_CAVITY_TOP - (top + P.UNDER_CELL_MAX + P.CELL_INSULATOR_T + P.CELL_T)
    res = gap - P.CELL_SWELL * P.CELL_T - poron_solid
    contact = "PORON preloads the cell" if gap < P.PORON_T else f"{gap - P.PORON_T:.2f} air above PORON (tape holds the cell)"
    check(f"PCB {t:.2f}: 10 % swell", res >= 0, f"gap {gap:.2f} - swell {P.CELL_SWELL * P.CELL_T:.2f} - PORON solid {poron_solid:.2f} = {res:+.3f}; {contact}")
say(f"  skin side and pogo pad face (z {P.Z_PCB_BOT - P.POGO_PAD_T:.2f}) do not move with PCB thickness")
say()

# ------------------------------------------------------------------ skin side
say("Skin side:")
for r in rows:
    if r["side"] == "skin" and not r["ref"].startswith("J1"):
        clr = P.SKIN_ZONE - r["z"] - 0.03
        check(f"skin part {r['ref']}", clr >= 0.05, f"h {r['z']:.2f}, clearance to window {clr:.2f} at rib -0.03")
rx0, rx1 = rib_led_pd1_span()
check(
    "LED|PD1 rib width",
    rx1 - rx0 >= 0.6,
    f"{rx1 - rx0:.2f} mm between D2 and PD1 bodies (window-plane frame {P.PD1_XY[0] - P.PD1_WIN / 2 - P.LED_XY[0] - P.LED_D / 2:.2f}); "
    "PC micro-moulding wants >= 0.6 [VERIFY with moulder]",
)
for typ_or_max, h in (("typ", P.U4_H_TYP), ("max", P.U4_H_MAX)):
    gap = P.SKIN_ZONE - h
    comp = 1 - gap / P.GAP_PAD_FREE_T
    check(f"TMP gap pad, U4 {typ_or_max} {h}", 0.10 <= comp <= 0.50, f"gap {gap:.2f}, pad {P.GAP_PAD_FREE_T} free -> {comp * 100:.0f} % compression")
say()

# ------------------------------------------------------------------ antenna and metal
say("Antenna and metal keep-out:")
ant_top = P.Z_PCB_TOP + P.ANT_LIFT + P.ANT_SIZE[2]
check("raised antenna room", ant_top <= P.Z_CAVITY_TOP - 0.3, f"top {ant_top:.2f} vs ceiling {P.Z_CAVITY_TOP:.2f}")
pod = pod_assembly().shape
metal = {"cell_301020", "tab_pos", "tab_neg", "thermal_button_316L"}
foamlike = {"poron_swell_pad", "pod_top_shell", "cell_insulator_pi_tape"}
for part in world_parts(pod):
    if part.label in metal | foamlike:
        xmax = part.bounding_box().max.X
        if part.label == "pod_top_shell":
            check("shell material", True, "plastic only, no inserts (by construction)")
            continue
        check(f"x < 24: {part.label}", xmax < P.ANT_KEEPOUT_X, f"max x {xmax:.2f}")
say()

# ------------------------------------------------------------------ mass
say("Mass (pod without adhesive):")
DENSITY = (  # g/cm3 [ASSUMED typical values]
    ("pod_top_shell", 1.20), ("window_frame", 1.20), ("clear_", 1.20), ("thermal_button", 8.00),
    ("thermal_gap_pad", 2.60), ("pogo_seal", 1.15), ("pod_pcb", 1.85), ("antenna_riser", 1.85),
    ("poron", 0.32), ("cell_insulator", 1.40), ("tab_", 8.90), ("adhesive", 0.0), ("liner", 0.0),
)
CELL_G = 2.0                 # LP301020 sheet: "Approx: 2g"
PART_RHO = 2.5               # component envelope boxes (overestimates: boxes are max envelopes)
total, rows_m = 0.0, {}
for part in world_parts(pod):
    lab = part.label or ""
    if lab == "cell_301020":
        g = CELL_G
    else:
        rho = next((r for k, r in DENSITY if k in lab), PART_RHO)
        g = part.volume / 1000.0 * rho
    key = next((k for k, _ in DENSITY if k in lab), "cell" if lab == "cell_301020" else "components")
    rows_m[key] = rows_m.get(key, 0.0) + g
    total += g
for k, g in sorted(rows_m.items(), key=lambda kv: -kv[1]):
    if g > 0.005:
        say(f"  {k:22s} {g:5.2f} g")
say(f"  {'total':22s} {total:5.2f} g")
from pod_shell import pod_comfort_dummy  # noqa: E402

RESIN_RHO, BALLAST_RHO = 1.15, 4.5   # SLA resin; steel shot in epoxy [ASSUMED]
dummy = pod_comfort_dummy().shape
resin_g = dummy.volume / 1000.0 * RESIN_RHO
need_g = total - resin_g
pocket_cc = 18.0 * 8.0 * 4.0 / 1000.0
fill = need_g / BALLAST_RHO / pocket_cc
check("comfort dummy can match mass", 0 <= fill <= 1.0, f"resin {resin_g:.2f} g + ballast {need_g:.2f} g ({fill * 100:.0f} % of the pocket at {BALLAST_RHO} g/cm3)")
say()

say(f"FAILED: {', '.join(fails) if fails else 'none'}")
OUT.parent.mkdir(exist_ok=True)
OUT.write_text("\n".join(lines) + "\n")
