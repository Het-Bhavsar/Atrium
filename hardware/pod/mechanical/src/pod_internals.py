"""Atrium pod internals: PCBA envelope, 301020 cell, PORON pad, adhesive die-cut.

The PCBA is an envelope model: the 28 x 14 x 0.6 stadium board plus one box per
placement row (centre, footprint and max height from pod_placement_rev_b1.csv, with
PD1 at x 5.7 and the VEMD5080X01 datasheet body 4 x 5 x 0.9). It is for clearance
checks, not a substitute for the EDA's 3D export.
"""

import build123d as bd
from cad import model

from lib import params as P
from lib.geom import box_at, cyl, pod_parts, rounded_rect_prism, rounded_square, stadium, stadium_prism

METAL_REFS = {"AE1"}         # the antenna itself is the only metal allowed at x >= 24


def part_box(r):
    x0, y0 = r["x"] - r["w"] / 2, r["y"] - r["h"] / 2
    if r["side"] == "top":
        return box_at(x0, y0, P.Z_PCB_TOP, r["w"], r["h"], max(r["z"], 0.02))
    return box_at(x0, y0, P.Z_PCB_BOT - max(r["z"], 0.02), r["w"], r["h"], max(r["z"], 0.02))


def pcba(ant_lift=P.ANT_LIFT):
    board = stadium_prism(P.PCB_L, P.PCB_W, P.Z_PCB_BOT, P.Z_PCB_TOP)
    board.label = "pod_pcb"
    board.color = P.FR4
    kids = [board]
    for r in pod_parts():
        ref = r["ref"]
        if ref.startswith("J1."):
            pad = cyl(P.POGO_PAD_D, P.Z_PCB_BOT - P.POGO_PAD_T, P.Z_PCB_BOT, r["x"], r["y"])
            pad.label = f"pad_{ref.replace('.', '_')}"
            pad.color = P.GOLD
            kids.append(pad)
            continue
        if ref == "AE1":
            continue
        b = part_box(r)
        b.label = ref.replace("+", "p").replace("-", "n")
        b.color = "#3b3b3b" if r["side"] == "top" else "#4a4a52"
        kids.append(b)

    ax, ay = P.ANT_XY
    sx, sy, sz = P.ANT_SIZE
    z_ant = P.Z_PCB_TOP
    if ant_lift > 0:
        rx, ry = P.ANT_RISER
        riser = box_at(ax - rx / 2, ay - ry / 2, P.Z_PCB_TOP, rx, ry, ant_lift)
        riser.label = "antenna_riser_fr4"
        riser.color = "#6e8f5a"
        kids.append(riser)
        z_ant += ant_lift
    ant = box_at(ax - sx / 2, ay - sy / 2, z_ant, sx, sy, sz)
    ant.label = "AE1"
    ant.color = "#e6e6e6"
    kids.append(ant)
    return bd.Compound(children=kids, label="pod_pcba")


@model
def pod_pcba():
    return pcba(P.ANT_LIFT)


@model
def pod_pcba_antenna_flat():
    return pcba(0.0)


def cell_parts(t=P.CELL_T, length=P.CELL_L, width=P.CELL_W):
    z0 = P.Z_CELL_BOT
    x0, y0 = P.CELL_X0, P.CELL_Y0
    body = rounded_rect_prism(x0, y0, x0 + length, y0 + width, P.CELL_CORNER_R, z0, z0 + t)
    body = bd.fillet(body.edges().group_by(bd.Axis.Z)[-1], 0.3)
    body.label = "cell_301020"
    body.color = P.CELL_COLOR

    tape = rounded_rect_prism(x0, y0, x0 + length, y0 + width, P.CELL_CORNER_R, z0 - P.CELL_INSULATOR_T, z0)
    tape.label = "cell_insulator_pi_tape"
    tape.color = "#c8762b"

    tabs = []
    zm = z0 + t / 2
    z_pad = P.Z_PCB_TOP + 0.1
    for name, y in zip(("tab_pos", "tab_neg"), P.CELL_TAB_Y):
        ty0 = y - P.CELL_TAB_W / 2
        fx = P.CELL_TAB_FOLD_X
        top = box_at(fx, ty0, zm - P.CELL_TAB_T / 2, x0 - fx, P.CELL_TAB_W, P.CELL_TAB_T)
        down = box_at(fx, ty0, z_pad, P.CELL_TAB_T, P.CELL_TAB_W, zm + P.CELL_TAB_T / 2 - z_pad)
        foot = box_at(fx, ty0, z_pad, P.CELL_TAB_PAD_X + 0.7 - fx, P.CELL_TAB_W, P.CELL_TAB_T)
        tab = top + down + foot
        tab.label = name
        tab.color = "#cfd2d6"
        tabs.append(tab)
    return [tape, body] + tabs


@model
def pod_cell():
    kids = cell_parts()
    cell = kids[1]
    bb = cell.bounding_box()
    assert bb.max.X < P.ANT_KEEPOUT_X, "cell foil inside the antenna keep-out"
    return bd.Compound(children=kids, label="cell_assembly")


@model
def pod_poron():
    i = P.PORON_INSET
    z0 = P.Z_CELL_BOT + P.CELL_T
    x0, y0 = P.CELL_X0 + i, P.CELL_Y0 + i
    body = rounded_rect_prism(x0, y0, P.CELL_X0 + P.CELL_L - i, P.CELL_Y0 + P.CELL_W - i, 0.5, z0, P.Z_CAVITY_TOP)
    body.label = "poron_swell_pad"
    body.color = P.PORON_COLOR
    assert body.bounding_box().max.X < P.ANT_KEEPOUT_X
    return body


def adhesive_holes(grow):
    return [
        bd.Pos(*P.LED_XY) * bd.Circle(P.LED_D / 2 + grow),
        rounded_square(P.PD1_WIN + 2 * grow, P.APERTURE_CORNER_R + grow, *P.PD1_XY),
        rounded_square(P.PD2_WIN + 2 * grow, P.APERTURE_CORNER_R + grow, *P.PD2_XY),
        bd.Pos(*P.TMP_XY) * bd.Circle(P.TMP_D / 2 + grow),
    ]


# The patch covers only the flat of the skin face, so no adhesive hangs under the edge round.
_INSET = P.SKIN_EDGE_FILLET + P.ADHESIVE_EDGE_INSET
ADHESIVE_OUTLINE = (P.POD_L - 2 * _INSET, P.POD_W - 2 * _INSET)
ADHESIVE_HOLE_GROW = 0.2                             # +0.2 per side over each aperture
LINER_TAB = (-5.0, 4.0, 1.0, 10.0)                   # pull tab at the low-x end, PCB coordinates


def adhesive_body(t, label):
    face = stadium(*ADHESIVE_OUTLINE)
    for h in adhesive_holes(ADHESIVE_HOLE_GROW):
        face = face - h
    body = bd.extrude(bd.Pos(0, 0, -t) * face, t)
    body.label = label
    body.color = P.ADHESIVE_COLOR
    return body


@model
def pod_adhesive():
    return adhesive_body(P.ADHESIVE_T, "adhesive_diecut")


@model
def pod_adhesive_foam05():
    """Wear-test option: 0.5 mm black medical foam tape, same die-cut."""
    return adhesive_body(P.ADHESIVE_FOAM_T[0], "adhesive_foam_0p5")


@model
def pod_adhesive_foam08():
    """Wear-test option: 0.8 mm black medical foam tape, same die-cut."""
    return adhesive_body(P.ADHESIVE_FOAM_T[1], "adhesive_foam_0p8")


@model
def pod_liner():
    face = stadium(*ADHESIVE_OUTLINE)
    x0, y0, x1, y1 = LINER_TAB
    face = face + bd.Pos((x0 + x1) / 2, (y0 + y1) / 2) * bd.RectangleRounded(x1 - x0, y1 - y0, 1.0)
    z0 = -P.ADHESIVE_T - P.LINER_T
    body = bd.extrude(bd.Pos(0, 0, z0) * face, P.LINER_T)
    body.label = "adhesive_liner_pull_tab"
    body.color = P.LINER_COLOR
    return body


if __name__ == "__main__":
    pod_pcba()
    pod_pcba_antenna_flat()
    pod_cell()
    pod_poron()
    pod_adhesive()
    pod_adhesive_foam05()
    pod_adhesive_foam08()
    pod_liner()
