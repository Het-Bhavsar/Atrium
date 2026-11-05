"""Atrium charging case with the pod docked skin-side down on pogo pins P1/P2."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "pod" / "mechanical" / "src"))

import build123d as bd
from cad import model

import case as C
from case import case_base, case_cell, case_cradle, case_hinge_pin, case_lid, case_magnet, case_pcba
from pod_assembly import pod_assembly_docked
from lib import params as P


def case_children(with_lid=True, with_pod=True, lid_angle=0.0):
    kids = [case_base().shape, case_cell().shape, case_pcba().shape, case_cradle().shape, case_hinge_pin().shape]
    if with_lid:
        axis = bd.Axis((C.HINGE_X, 0, C.HINGE_Z), (0, 1, 0))
        for part in (case_lid().shape, case_magnet().shape):
            moved = part.rotate(axis, -lid_angle) if lid_angle else part
            moved.label = part.label
            kids.append(moved)
    if with_pod:
        dx, dy = P.POD_DOCK_OFFSET
        pod = bd.Pos(dx, dy, P.Z_CRADLE_FLOOR) * pod_assembly_docked().shape
        pod.label = "atrium_pod_docked"
        kids.append(pod)
    return kids


@model
def case_assembly():
    return bd.Compound(children=case_children(), label="atrium_case")


@model
def case_assembly_open():
    """Lid open 105 degrees about the hinge axis."""
    return bd.Compound(children=case_children(lid_angle=105.0), label="atrium_case")


@model
def case_assembly_lid_off():
    return bd.Compound(children=case_children(with_lid=False), label="atrium_case")


if __name__ == "__main__":
    case_assembly()
    case_assembly_open()
    case_assembly_lid_off()
