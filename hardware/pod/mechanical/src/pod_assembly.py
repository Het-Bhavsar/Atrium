"""Atrium pod assembly, pod frame (PCB lower-left origin, z = 0 at the skin face)."""

import build123d as bd
from cad import model

from pod_internals import pod_adhesive, pod_cell, pod_liner, pod_pcba, pod_poron
from pod_shell import pod_top_shell
from pod_window import pod_window_clear, pod_window_frame, pogo_gasket, thermal_button, thermal_gap_pad
from lib import params as P


def pod_children(with_adhesive=True):
    kids = [
        pod_top_shell().shape,
        pod_window_frame().shape,
        pod_window_clear().shape,
        thermal_button().shape,
        thermal_gap_pad().shape,
        pod_pcba().shape,
        pod_cell().shape,
        pod_poron().shape,
    ]
    if P.POGO_SEAL == "gasket":
        kids.append(pogo_gasket().shape)
    if with_adhesive:
        kids += [pod_adhesive().shape, pod_liner().shape]
    return kids


@model
def pod_assembly():
    pod = bd.Compound(children=pod_children(), label="atrium_pod")
    bb = pod.bounding_box()
    assert abs(bb.max.Z - P.POD_H) < 1e-6, bb.max.Z
    return pod


@model
def pod_assembly_docked():
    """Pod as it sits in the case: adhesive and liner removed."""
    return bd.Compound(children=pod_children(with_adhesive=False), label="atrium_pod")


if __name__ == "__main__":
    pod_assembly()
    pod_assembly_docked()
