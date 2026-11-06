"""Atrium pod top shell: stadium 30 x 16 x 7.0, 0.5 top, 1.0 nominal wall.

The shell wraps the window plate with a 0.5 mm skirt (z 0-0.5). Above the window the
cavity is the PCB outline plus PCB_WALL_CLEAR, so the wall is 0.9 at the PCB band.
Hold-down tabs reach from the ceiling to the PCB top along the 0.3 mm edge band,
clamping the PCB onto the window ribs. Plastic only: nothing metal at x >= 24.

Visible (ornamental) surfaces: TOP_EDGE_FILLET, SKIN_EDGE_FILLET and the flat top face.
They come from comfort and moulding needs and stay parameters; checks/design_rules.py
guards the overall form.
"""

import build123d as bd
from cad import model

from lib import params as P
from lib.geom import box_at, stadium_prism, union

HOLD_DOWN_BAND = 0.25        # overlap onto the PCB edge band
HOLD_DOWN_LEN = 2.0
HOLD_DOWN_SIDE_X = (9.0, 19.0)


def shell_outer(height=P.POD_H):
    """The pod's outer envelope: flat top, straight walls, rounded top and skin edges."""
    outer = stadium_prism(P.POD_L, P.POD_W, 0, height)
    outer = bd.fillet(outer.edges().group_by(bd.Axis.Z)[-1], P.TOP_EDGE_FILLET)
    return bd.fillet(outer.edges().group_by(bd.Axis.Z)[0], P.SKIN_EDGE_FILLET)


def shell_body(height=P.POD_H):
    outer = shell_outer(height)

    cavity_top = height - P.SHELL_TOP_T
    skirt = stadium_prism(P.POD_L - 2 * P.SKIRT_T, P.POD_W - 2 * P.SKIRT_T, -0.1, P.Z_WINDOW_TOP)
    inner_l = P.PCB_L + 2 * P.PCB_WALL_CLEAR
    inner_w = P.PCB_W + 2 * P.PCB_WALL_CLEAR
    cavity = stadium_prism(inner_l, inner_w, P.Z_WINDOW_TOP - 0.01, cavity_top)
    body = outer - skirt - cavity

    band = stadium_prism(inner_l + 0.02, inner_w + 0.02, P.Z_PCB_TOP, cavity_top + 0.01) - stadium_prism(
        P.PCB_L - 2 * HOLD_DOWN_BAND, P.PCB_W - 2 * HOLD_DOWN_BAND, P.Z_PCB_TOP - 0.1, cavity_top + 0.1
    )
    zone_h = cavity_top - P.Z_PCB_TOP + 0.02
    keys = []
    for x in HOLD_DOWN_SIDE_X:
        for y0 in (-1.0, P.PCB_W - 1.0):
            keys.append(box_at(x - HOLD_DOWN_LEN / 2, y0, P.Z_PCB_TOP, HOLD_DOWN_LEN, 2.0, zone_h))
    keys.append(box_at(-1.0, P.PCB_CY - HOLD_DOWN_LEN / 2, P.Z_PCB_TOP, 2.0, HOLD_DOWN_LEN, zone_h))
    keys.append(box_at(P.PCB_L - 1.0, P.PCB_CY - HOLD_DOWN_LEN / 2, P.Z_PCB_TOP, 2.0, HOLD_DOWN_LEN, zone_h))
    tabs = band & union(keys)
    body = body + tabs

    body.label = "pod_top_shell"
    body.color = P.SHELL_COLOR
    return body


DUMMY_POCKET = (18.0, 8.0, 4.0)   # ballast pocket from the top, over the cell zone


@model
def pod_comfort_dummy():
    """Wear-test dummy: the outer envelope as one solid, with a top pocket for ballast.

    Print in SLA resin; fill the pocket with steel shot in epoxy until the dummy weighs what
    checks/stackup.py reports for the real pod; seal flush. The skin face is the real one.
    """
    body = shell_outer()
    l, w, d = DUMMY_POCKET
    x0, y0 = P.CELL_X0 + (P.CELL_L - l) / 2, P.CELL_Y0 + (P.CELL_W - w) / 2
    pocket = bd.extrude(bd.Pos(x0 + l / 2, y0 + w / 2, P.POD_H - d) * bd.RectangleRounded(l, w, 1.5), d + 0.1)
    body = body - pocket
    body.label = "pod_comfort_dummy"
    body.color = "#d9d4cb"
    return body


@model
def pod_top_shell():
    body = shell_body()
    bb = body.bounding_box()
    assert abs(bb.size.X - P.POD_L) < 1e-3 and abs(bb.size.Y - P.POD_W) < 1e-3
    assert abs(bb.size.Z - P.POD_H) < 1e-3, bb.size
    return body


if __name__ == "__main__":
    pod_top_shell()
    pod_comfort_dummy()
