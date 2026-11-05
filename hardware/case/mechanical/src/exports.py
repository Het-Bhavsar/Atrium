"""Atrium charging case mechanical deliverables. Rewrites every file on each run.

STEP: every case part and the case assemblies (pod docked). STL + 3MF: the print set.

Usage: cad run exports.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "pod" / "mechanical" / "src"))

from cad import export

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

if __name__ == "__main__":
    for name, fn in STEP_PARTS.items():
        export(fn(), ROOT / f"STEP/atrium_{name}_{REV}.step")
    for name, fn in PRINT_PARTS.items():
        export(fn(), ROOT / f"STL/atrium_{name}_{REV}.stl", ROOT / f"3MF/atrium_{name}_{REV}.3mf", tolerance=0.01)
