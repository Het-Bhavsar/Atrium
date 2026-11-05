"""Atrium pod skin window: black PC frame + clear PC apertures (two-shot), 0.5 mm.

The frame plate sits in the shell skirt. Above it a perimeter rim, three light-barrier
ribs and the pogo sealing bar rise to the PCB skin face and set the PCB height. Each
clear aperture is an island ringed by black, so no light path runs through window
plastic. The TMP117 bore holds a 316L button over a 0.2 mm gap pad.

LED|PD1 rib: the window plane has 0.8 mm of frame between the apertures, but the D2
and PD1 bodies are only ~0.55 mm apart at the PCB, so the rib above the plate is
limited to the gap between bodies (see rib_led_pd1_span). Flagged in README.
"""

import build123d as bd
from cad import model

from lib import params as P
from lib.geom import box_at, cyl, part_row, pod_parts, rounded_square, stadium, stadium_prism, union

Z0, Z1, Z2 = 0.0, P.Z_WINDOW_TOP, P.Z_PCB_BOT


def plate_outline():
    return (P.POD_L - 2 * P.SKIRT_T - 2 * P.WINDOW_FIT_CLEAR, P.POD_W - 2 * P.SKIRT_T - 2 * P.WINDOW_FIT_CLEAR)


def rib_led_pd1_span():
    """x span of the LED|PD1 rib above the plate: between the two part bodies."""
    rows = pod_parts()
    d2, pd1 = part_row(rows, "D2"), part_row(rows, "PD1")
    x0 = d2["x"] + d2["w"] / 2 + P.PART_CLEAR
    x1 = pd1["x"] - pd1["w"] / 2 - P.PART_CLEAR
    return x0, x1


def aperture_faces():
    """Clear apertures as faces in the window plane (TMP bore handled separately)."""
    return {
        "led": bd.Pos(*P.LED_XY) * bd.Circle(P.LED_D / 2),
        "pd1": rounded_square(P.PD1_WIN, P.APERTURE_CORNER_R, *P.PD1_XY),
        "pd2": rounded_square(P.PD2_WIN, P.APERTURE_CORNER_R, *P.PD2_XY),
    }


def pogo_bar_height():
    if P.POGO_SEAL == "gasket":
        return Z2 - P.POGO_GASKET_T
    return Z2


def pogo_bar(z_top):
    xs = [p[0] for p in P.POGO_PADS]
    y = P.POGO_PADS[0][1]
    x0, x1 = min(xs) - P.POGO_BAR_END, max(xs) + P.POGO_BAR_END
    y0 = y - P.POGO_BAR_W / 2
    y1 = P.PCB_W - P.RIM_W + 0.05       # merges into the rim
    sk = bd.Pos((x0 + x1) / 2, (y0 + y1) / 2) * bd.RectangleRounded(x1 - x0, y1 - y0, 0.3)
    return bd.extrude(bd.Pos(0, 0, Z1 - 0.01) * sk, z_top - Z1 + 0.01)


def frame_body():
    pl, pw = plate_outline()
    plate = stadium_prism(pl, pw, Z0, Z1)

    rim_out = (P.PCB_L + 2 * P.PCB_WALL_CLEAR - 2 * P.WINDOW_FIT_CLEAR, P.PCB_W + 2 * P.PCB_WALL_CLEAR - 2 * P.WINDOW_FIT_CLEAR)
    rim = stadium_prism(*rim_out, Z1 - 0.01, Z2) - stadium_prism(P.PCB_L - 2 * P.RIM_W, P.PCB_W - 2 * P.RIM_W, Z1 - 0.1, Z2 + 0.1)

    inside = stadium_prism(P.PCB_L - 2 * P.RIM_W + 0.1, P.PCB_W - 2 * P.RIM_W + 0.1, Z1 - 0.01, Z2)
    rx0, rx1 = rib_led_pd1_span()
    ribs = [
        box_at(rx0, -1, Z1 - 0.01, rx1 - rx0, P.PCB_W + 2, Z2 - Z1 + 0.01),
        box_at(P.RIB_PD1_TMP_X - P.RIB_W / 2, -1, Z1 - 0.01, P.RIB_W, P.PCB_W + 2, Z2 - Z1 + 0.01),
        box_at(P.RIB_TMP_PD2_X - P.RIB_W / 2, -1, Z1 - 0.01, P.RIB_W, P.PCB_W + 2, Z2 - Z1 + 0.01),
    ]
    body = plate + rim + (union(ribs) & inside) + pogo_bar(pogo_bar_height())
    if P.POGO_SEAL == "gasket":
        body = body - (pogo_bar(Z2 + 0.1) & box_at(-5, -5, pogo_bar_height(), 40, 30, 1.0))

    for face in aperture_faces().values():
        body = body - bd.extrude(bd.Pos(0, 0, Z0 - 0.1) * face, Z1 - Z0 + 0.2)
    body = body - cyl(P.TMP_D, Z0 - 0.1, Z1 + 0.1, *P.TMP_XY)
    body = body - cyl(P.BUTTON_FLANGE_D + 2 * P.BUTTON_CLEAR, Z1 - P.BUTTON_FLANGE_T, Z1 + 0.1, *P.TMP_XY)
    for x, y in P.POGO_PADS:
        body = body - cyl(P.POGO_HOLE_D, Z0 - 0.1, Z2 + 0.1, x, y)
    return body


@model
def pod_window_frame():
    body = frame_body()
    body.label = "window_frame_black_pc"
    body.color = P.BLACK_PC
    bb = body.bounding_box()
    assert abs(bb.min.Z - Z0) < 1e-6 and abs(bb.max.Z - Z2) < 1e-6, (bb.min.Z, bb.max.Z)
    return body


@model
def pod_window_clear():
    parts = []
    for name, face in aperture_faces().items():
        p = bd.extrude(bd.Pos(0, 0, Z0) * face, Z1 - Z0)
        p.label = f"clear_{name}"
        p.color = P.CLEAR_PC
        parts.append(p)
    return bd.Compound(children=parts, label="window_clear_pc")


@model
def thermal_button():
    d = P.TMP_D - 2 * P.BUTTON_CLEAR
    body = cyl(d, Z0, Z1, *P.TMP_XY) + cyl(P.BUTTON_FLANGE_D, Z1 - P.BUTTON_FLANGE_T, Z1, *P.TMP_XY)
    body = bd.fillet(body.edges().group_by(bd.Axis.Z)[0], 0.1)
    body.label = "thermal_button_316L"
    body.color = P.STEEL
    return body


@model
def thermal_gap_pad():
    x, y = P.TMP_XY
    body = box_at(x - P.GAP_PAD_SIZE / 2, y - P.GAP_PAD_SIZE / 2, Z1, P.GAP_PAD_SIZE, P.GAP_PAD_SIZE, P.GAP_PAD_T)
    body.label = "thermal_gap_pad"
    body.color = "#7fb3d5"
    return body


@model
def pogo_gasket():
    """Silicone seal strip between the pogo bar and the PCB solder mask."""
    z0 = pogo_bar_height()
    if P.POGO_SEAL != "gasket":
        body = box_at(0, 0, z0, 0.01, 0.01, 0.01)
    else:
        bar = pogo_bar(Z2)
        body = bar & box_at(-5, -5, z0, 40, 30, Z2 - z0)
        for x, y in P.POGO_PADS:
            body = body - cyl(P.POGO_HOLE_D, z0 - 0.1, Z2 + 0.1, x, y)
    body.label = "pogo_seal_gasket"
    body.color = "#5a1f1f"      # black silicone in production; dark red to read in renders
    return body


if __name__ == "__main__":
    pod_window_frame()
    pod_window_clear()
    thermal_button()
    thermal_gap_pad()
    pogo_gasket()
