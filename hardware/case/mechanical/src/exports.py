"""Atrium charging case mechanical deliverables. Rewrites every file on each run.

STEP: every case part and the case assemblies (pod docked). STL + 3MF: the print set.
DXF: case PCB outline with the mechanical keep-outs and targets, case PCB frame.

Usage: cad run exports.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "pod" / "mechanical" / "src"))

import build123d as bd
from cad import export

import case as C
from lib import params as P
from lib.geom import case_parts, stadium

from case import case_base, case_cell, case_cradle, case_hinge_pin, case_lid, case_magnet, case_pcba
from case_assembly import case_assembly, case_assembly_lid_off, case_assembly_open

ROOT = Path(__file__).resolve().parent.parent
REV = "revB1-1"
STEP_PARTS = {
    "case_base": case_base,
    "case_cradle": case_cradle,
    "case_lid": case_lid,
    "case_magnet": case_magnet,
    "case_hinge_pin": case_hinge_pin,
    "case_pcba_envelope": case_pcba,
    "case_cell_LP602525": case_cell,
    "case_assembly": case_assembly,
    "case_assembly_open": case_assembly_open,
    "case_assembly_lid_off": case_assembly_lid_off,
}
PRINT_PARTS = {"case_base": case_base, "case_cradle": case_cradle, "case_lid": case_lid}


def fixed_dxf_dates(path):
    """Pin the DXF header timestamps so re-exports only change when geometry does."""
    import re

    lines = open(path).read().split("\n")
    for i, line in enumerate(lines[:-2]):
        if line.strip() in ("$TDCREATE", "$TDUCREATE", "$TDUPDATE", "$TDUUPDATE", "$TDINDWG", "$TDUSRTIMER"):
            lines[i + 2] = "0.0"
        elif line.strip() in ("$FINGERPRINTGUID", "$VERSIONGUID"):
            lines[i + 2] = "{00000000-0000-0000-0000-000000000000}"
    text = re.sub(r" @ \d{4}-\d\d-\d\dT[\d:.+]+", "", "\n".join(lines))
    open(path, "w").write(text)


def rrect(x0, y0, x1, y1, r):
    return bd.Pos((x0 + x1) / 2, (y0 + y1) / 2) * bd.RectangleRounded(x1 - x0, y1 - y0, r)


def keepout_dxf(path):
    """Case PCB keep-outs and targets (top side unless noted)."""
    rows = {r["ref"]: r for r in case_parts()}
    j1 = rows["J1"]
    hx, hy = P.HALL_XY
    (x1, y1), (x2, y2) = P.CASE_POGO
    layers = {
        "BOARD_OUTLINE": (bd.ColorIndex.GRAY, [rrect(0, 0, P.CASE_PCB_L, P.CASE_PCB_W, P.CASE_PCB_R)]),
        "TOP_PARTS_ZONE_MAX_1p1": (bd.ColorIndex.GREEN, [rrect(1.0, 1.0, P.CASE_PCB_L - 1.0, P.CASE_PCB_W - 1.0, 2.0)]),
        "J1_USB_C": (bd.ColorIndex.CYAN, [rrect(j1["x"] - j1["w"] / 2, 0, j1["x"] + j1["w"] / 2, j1["h"], 0.01)]),
        "POGO_PINS": (bd.ColorIndex.YELLOW, [bd.Pos(x, y) * bd.Circle(P.POGO_PIN_D / 2) for x, y in P.CASE_POGO]),
        "POGO_BARREL_SLOT": (bd.ColorIndex.YELLOW, [bd.Pos((x1 + x2) / 2, y1) * bd.SlotOverall(x2 - x1 + P.POGO_PIN_D + 0.4, P.POGO_PIN_D + 0.4)]),
        "HALL_U4": (bd.ColorIndex.MAGENTA, [rrect(hx - 1.5, hy - 1.4, hx + 1.5, hy + 1.4, 0.01)]),
        "MAGNET_AXIS": (bd.ColorIndex.MAGENTA, [bd.Pos(*P.MAGNET_XY) * bd.Circle(P.MAGNET_D / 2)]),
        "LED_LIGHT_HOLES": (bd.ColorIndex.RED, [bd.Pos(rows[k]["x"], rows[k]["y"]) * bd.Circle(0.8) for k in ("LED1", "LED2")]),
        "POD_POCKET": (bd.ColorIndex.BLUE, [stadium(P.POD_L + 2 * P.CRADLE_POCKET_CLEAR, P.POD_W + 2 * P.CRADLE_POCKET_CLEAR, C.POD_CX, C.POD_CY)]),
        "BOTTOM_NO_PARTS_CELL": (bd.ColorIndex.GRAY, [rrect(P.CASE_CELL_X0, 0.25, P.CASE_PCB_L, P.CASE_PCB_W - 0.25, 0.8)]),
        "BOTTOM_NO_PARTS_SUPPORT": (bd.ColorIndex.GRAY, [rrect(1.0, 0.0, 2.0, P.CASE_PCB_W, 0.01), rrect(P.CASE_CELL_X0 - 1.6, 0.0, P.CASE_CELL_X0 - 0.6, P.CASE_PCB_W, 0.01)]),
    }
    dxf = bd.ExportDXF(unit=bd.Unit.MM)
    for name, (color, shapes) in layers.items():
        dxf.add_layer(name, color=color)
        for sh in shapes:
            dxf.add_shape(sh.wire() if hasattr(sh, "wire") else sh, layer=name)
    dxf.write(path)
    fixed_dxf_dates(path)

if __name__ == "__main__":
    for name, fn in STEP_PARTS.items():
        export(fn(), ROOT / f"STEP/atrium_{name}_{REV}.step")
    for name, fn in PRINT_PARTS.items():
        export(fn(), ROOT / f"STL/atrium_{name}_{REV}.stl", ROOT / f"3MF/atrium_{name}_{REV}.3mf", tolerance=0.01)
    (ROOT / "DXF").mkdir(exist_ok=True)
    keepout_dxf(str(ROOT / f"DXF/atrium_case_pcb_keepouts_{REV}.dxf"))
