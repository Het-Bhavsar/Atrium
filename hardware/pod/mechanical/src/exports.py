"""Atrium pod mechanical deliverables. Rewrites every file on each run.

STEP: every pod part and the pod assemblies. STL + 3MF: the test-print set.
DXF: window apertures and the adhesive die-cut, in the shell frame
(shell lower-left origin = PCB frame + (1, 1)).

Usage: cad run exports.py
"""

from pathlib import Path

import build123d as bd
from cad import export

from lib import params as P
from lib.geom import stadium
from pod_assembly import pod_assembly, pod_assembly_docked
from pod_internals import (
    ADHESIVE_HOLE_GROW,
    ADHESIVE_OUTLINE,
    LINER_TAB,
    adhesive_holes,
    pod_adhesive,
    pod_adhesive_foam05,
    pod_adhesive_foam08,
    pod_cell,
    pod_liner,
    pod_pcba,
    pod_pcba_antenna_flat,
    pod_poron,
)
from pod_shell import pod_comfort_dummy, pod_tab_pads, pod_top_shell
from pod_window import aperture_faces, pod_window_clear, pod_window_frame, pogo_gasket, thermal_button, thermal_gap_pad

ROOT = Path(__file__).resolve().parent.parent
REV = "revB1-1"
STEP_PARTS = {
    "pod_top_shell": pod_top_shell,
    "pod_window_frame": pod_window_frame,
    "pod_window_clear": pod_window_clear,
    "pod_thermal_button": thermal_button,
    "pod_thermal_gap_pad": thermal_gap_pad,
    "pod_pogo_gasket": pogo_gasket,
    "pod_tab_pads": pod_tab_pads,
    "pod_pcba_envelope": pod_pcba,
    "pod_pcba_envelope_antenna_flat": pod_pcba_antenna_flat,
    "pod_cell_301020": pod_cell,
    "pod_poron_pad": pod_poron,
    "pod_adhesive_diecut": pod_adhesive,
    "pod_adhesive_foam_0p5": pod_adhesive_foam05,
    "pod_adhesive_foam_0p8": pod_adhesive_foam08,
    "pod_comfort_dummy": pod_comfort_dummy,
    "pod_adhesive_liner": pod_liner,
    "pod_assembly": pod_assembly,
    "pod_assembly_docked": pod_assembly_docked,
}
PRINT_PARTS = {
    "pod_top_shell": pod_top_shell,
    "pod_window_frame": pod_window_frame,
    "pod_comfort_dummy": pod_comfort_dummy,
}


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


def to_dxf_frame(shape):
    return bd.Pos(1.0, 1.0) * shape


def write_dxf(path, layers):
    dxf = bd.ExportDXF(unit=bd.Unit.MM)
    for name, (color, shapes) in layers.items():
        dxf.add_layer(name, color=color)
        for s in shapes:
            dxf.add_shape(to_dxf_frame(s), layer=name)
    dxf.write(path)
    fixed_dxf_dates(path)


def dxf_window(path):
    ap = aperture_faces()
    tmp = bd.Pos(*P.TMP_XY) * bd.Circle(P.TMP_D / 2)
    pogo = [bd.Pos(x, y) * bd.Circle(P.POGO_HOLE_D / 2) for x, y in P.POGO_PADS]
    write_dxf(
        path,
        {
            "SHELL_OUTLINE": (bd.ColorIndex.GRAY, [stadium(P.POD_L, P.POD_W).wire()]),
            "PCB_OUTLINE": (bd.ColorIndex.GREEN, [stadium(P.PCB_L, P.PCB_W).wire()]),
            "WINDOW_APERTURES": (bd.ColorIndex.CYAN, [f.wire() for f in ap.values()] + [tmp.wire()]),
            "POGO_HOLES": (bd.ColorIndex.YELLOW, [p.wire() for p in pogo]),
        },
    )


def dxf_adhesive(path):
    x0, y0, x1, y1 = LINER_TAB
    tab = bd.Pos((x0 + x1) / 2, (y0 + y1) / 2) * bd.RectangleRounded(x1 - x0, y1 - y0, 1.0)
    write_dxf(
        path,
        {
            "CUT_OUTLINE": (bd.ColorIndex.GRAY, [stadium(*ADHESIVE_OUTLINE).wire()]),
            "CUT_HOLES": (bd.ColorIndex.CYAN, [h.wire() for h in adhesive_holes(ADHESIVE_HOLE_GROW)]),
            "LINER_TAB": (bd.ColorIndex.YELLOW, [tab.wire()]),
        },
    )


if __name__ == "__main__":
    for name, fn in STEP_PARTS.items():
        export(fn(), ROOT / f"STEP/atrium_{name}_{REV}.step")
    for name, fn in PRINT_PARTS.items():
        export(fn(), ROOT / f"STL/atrium_{name}_{REV}.stl", ROOT / f"3MF/atrium_{name}_{REV}.3mf", tolerance=0.01)
    (ROOT / "DXF").mkdir(exist_ok=True)
    dxf_window(str(ROOT / f"DXF/atrium_pod_window_apertures_{REV}.dxf"))
    dxf_adhesive(str(ROOT / f"DXF/atrium_adhesive_diecut_{REV}.dxf"))
