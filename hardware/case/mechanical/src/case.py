"""Atrium charging case: base, cradle insert, hinged lid, PCBA envelope, cell, magnet.

Case frame: origin at the case PCB lower-left, z = 0 at the outer bottom face.
Stack: floor 1.0 | LP602525 cell pocket 7.2 (6.3 max + swell + foam) | PCB 1.0 | cradle | pod | lid.
The cradle floor height comes from the pogo working height: the pod pads sit
POD_PAD_DEPTH above the pod skin face, and the pins are compressed POGO_WORK_COMP.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "pod" / "mechanical" / "src"))

import math

import build123d as bd
from cad import model

from lib import params as P
from lib.geom import box_at, case_parts, cyl, rounded_rect_prism, stadium, stadium_prism

X0, X1 = P.CASE_IN_X0, P.CASE_IN_X1
Y0, Y1 = P.CASE_IN_Y0, P.CASE_IN_Y1
IN_R = P.CASE_PCB_R + P.CASE_PCB_CLEAR
OX0, OX1 = X0 - P.CASE_WALL, X1 + P.CASE_WALL
OY0, OY1 = Y0 - P.CASE_WALL, Y1 + P.CASE_WALL
DX, DY = P.POD_DOCK_OFFSET
POD_CX, POD_CY = P.PCB_CX + DX, P.PCB_CY + DY

# Barrel-spine hinge along the straight part of the x-min end wall: five knuckles
# (base, lid, base, lid, base), axis on the outer wall face at the parting line, so every
# lid point sits at or inside the axis and swings up. The pin is flush and hidden.
HINGE_X = OX0
HINGE_Z = P.Z_CASE_RIM
KNUCKLE_D = 3.0
HINGE_PIN_D = 1.0
KNUCKLE_GAP = 0.25
KNUCKLE_CLEAR = 0.2
HINGE_Y0, HINGE_Y1 = OY0 + P.CASE_OUT_R, OY1 - P.CASE_OUT_R
LID_LIP_W = 0.8
LID_LIP_H = 0.8
MAG_BOSS_D = 4.0
MAG_ROOF_T = 0.3             # cradle roof over the hall sensor
MAG_FLOOR_T = 0.3            # lid boss wall under the magnet
MAG_BOSS_CLEAR = 0.05

Z_RELIEF = P.Z_CASE_PCB_TOP + P.CASE_PARTS_MAX_H + P.CASE_CRADLE_UNDER_CLEAR
Z_MAG_POCKET = Z_RELIEF + MAG_ROOF_T
Z_MAG_BOSS_BOT = Z_MAG_POCKET + MAG_BOSS_CLEAR
Z_MAGNET_BOT = Z_MAG_BOSS_BOT + MAG_FLOOR_T


def outer_prism(z0, z1):
    return rounded_rect_prism(OX0, OY0, OX1, OY1, P.CASE_OUT_R, z0, z1)


def inner_prism(z0, z1, grow=0.0):
    return rounded_rect_prism(X0 - grow, Y0 - grow, X1 + grow, Y1 + grow, IN_R + grow, z0, z1)


def knuckle(y0, y1):
    k = bd.Pos(HINGE_X, (y0 + y1) / 2, HINGE_Z) * bd.Rot(90, 0, 0) * bd.Cylinder(KNUCKLE_D / 2, y1 - y0)
    return k


def knuckle_spans():
    n = 5
    seg = (HINGE_Y1 - HINGE_Y0 - (n - 1) * KNUCKLE_GAP) / n
    spans = [(HINGE_Y0 + i * (seg + KNUCKLE_GAP), HINGE_Y0 + i * (seg + KNUCKLE_GAP) + seg) for i in range(n)]
    return spans[0::2], spans[1::2]


def knuckle_clearance(y0, y1):
    """Room for the other half's knuckle, with KNUCKLE_CLEAR all round."""
    return bd.Pos(HINGE_X, (y0 + y1) / 2, HINGE_Z) * bd.Rot(90, 0, 0) * bd.Cylinder(
        KNUCKLE_D / 2 + KNUCKLE_CLEAR, y1 - y0 + 2 * KNUCKLE_GAP
    )


def pin_hole():
    return bd.Pos(HINGE_X, (HINGE_Y0 + HINGE_Y1) / 2, HINGE_Z) * bd.Rot(90, 0, 0) * bd.Cylinder(HINGE_PIN_D / 2 + 0.05, HINGE_Y1 - HINGE_Y0 + 0.2)


@model
def case_base():
    body = outer_prism(0, P.Z_CASE_RIM) - inner_prism(P.CASE_FLOOR_T, P.Z_CASE_RIM + 1)
    body = bd.fillet(body.edges().group_by(bd.Axis.Z)[0], 0.8)

    # PCB supports where the cell is not (x < cell start) plus the cell foam elsewhere
    z_sup = P.Z_CASE_PCB_BOT
    ledge = inner_prism(P.CASE_FLOOR_T - 0.01, z_sup) - inner_prism(P.CASE_FLOOR_T - 0.1, z_sup + 0.1, grow=-0.8)
    ledge = ledge & box_at(X0 - 1, Y0 - 1, 0, P.CASE_CELL_X0 - 0.4 - X0 + 1, Y1 - Y0 + 2, z_sup + 1)
    ribs = box_at(1.0, Y0, P.CASE_FLOOR_T - 0.01, 1.0, Y1 - Y0, z_sup - P.CASE_FLOOR_T + 0.01) + box_at(
        P.CASE_CELL_X0 - 1.6, Y0, P.CASE_FLOOR_T - 0.01, 1.0, Y1 - Y0, z_sup - P.CASE_FLOOR_T + 0.01
    )
    body = body + ledge + (ribs & inner_prism(0, z_sup + 1))

    # USB-C: through opening plus an outside recess for the plug overmold [VERIFY]
    uw, uh = P.USB_OPEN
    uz = P.Z_CASE_PCB_TOP + P.USB_H / 2
    slot = bd.Pos(20.0, OY0 - 0.1, uz) * bd.Rot(-90, 0, 0) * bd.extrude(bd.SlotOverall(uw, uh), P.CASE_WALL + 0.4)
    recess = bd.Pos(20.0, OY0 - 0.1, uz) * bd.Rot(-90, 0, 0) * bd.extrude(bd.RectangleRounded(12.6, 6.8, 2.0), 0.9)
    body = body - slot - recess

    base_k, lid_k = knuckle_spans()
    for y0, y1 in lid_k:
        body = body - knuckle_clearance(y0, y1)
    for y0, y1 in base_k:
        body = body + knuckle(y0, y1)
    body = body - pin_hole()
    body.label = "case_base"
    body.color = P.CASE_COLOR
    return body


def pod_pocket(z0, z1, clear=P.CRADLE_POCKET_CLEAR):
    return stadium_prism(P.POD_L + 2 * clear, P.POD_W + 2 * clear, z0, z1, POD_CX, POD_CY)


@model
def case_cradle():
    z0, z1 = P.Z_CASE_PCB_TOP, P.Z_CASE_RIM
    body = inner_prism(z0, z1, grow=-0.1)
    # underside relief over the PCB parts, keeping a 1.0 mm band on the PCB edge
    relief = rounded_rect_prism(1.0, 1.0, P.CASE_PCB_L - 1.0, P.CASE_PCB_W - 1.0, 2.0, z0 - 0.1, Z_RELIEF)
    body = body - relief
    # lid lip rebate around the top
    rebate = inner_prism(z1 - LID_LIP_H, z1 + 0.1) - inner_prism(z1 - LID_LIP_H - 0.1, z1 + 0.2, grow=-(LID_LIP_W + 0.1))
    body = body - rebate
    # pod pocket with lead-in
    body = body - pod_pocket(P.Z_CRADLE_FLOOR, z1 + 0.1)
    lead = bd.extrude(
        bd.Pos(0, 0, z1 - P.CRADLE_LEADIN) * stadium(P.POD_L + 2 * P.CRADLE_POCKET_CLEAR, P.POD_W + 2 * P.CRADLE_POCKET_CLEAR, POD_CX, POD_CY),
        P.CRADLE_LEADIN + 0.01,
        taper=-45,
    )
    body = body - lead
    # finger scoop on the +y side
    body = body - cyl(9.0, P.Z_CRADLE_FLOOR + 1.0, z1 + 0.1, POD_CX, POD_CY + P.POD_W / 2 + 2.0)
    # pogo barrels pass the floor
    (x1, y1), (x2, y2) = P.CASE_POGO
    slot = bd.Pos((x1 + x2) / 2, y1) * bd.SlotOverall(x2 - x1 + P.POGO_PIN_D + 0.4, P.POGO_PIN_D + 0.4)
    body = body - bd.extrude(bd.Pos(0, 0, z0 - 0.1) * slot, z1 - z0 + 0.2)
    # parts taller than the relief: J1 (USB-C), with 0.2 clearance
    j1 = [r for r in case_parts() if r["ref"] == "J1"][0]
    body = body - box_at(j1["x"] - j1["w"] / 2 - 0.2, -1.0, z0 - 0.1, j1["w"] + 0.4, j1["h"] + 1.2, j1["z"] + 0.3)
    # magnet pocket over the hall sensor, and light holes over LED1/LED2
    mx, my = P.MAGNET_XY
    body = body - cyl(MAG_BOSS_D + 2 * MAG_BOSS_CLEAR, Z_MAG_POCKET, z1 + 0.1, mx, my)
    for r in case_parts():
        if r["ref"] in ("LED1", "LED2"):
            body = body - cyl(1.6, z0, z1 + 0.1, r["x"], r["y"])
    body.label = "case_cradle"
    body.color = "#b9b4ab"
    return body


@model
def case_lid():
    z0, z1 = P.Z_CASE_RIM, P.Z_LID_TOP
    body = outer_prism(z0, z1) - inner_prism(z0 - 0.1, P.Z_LID_INNER)
    body = bd.fillet(body.edges().group_by(bd.Axis.Z)[-1], 1.5)
    lip = inner_prism(z0 - LID_LIP_H, z0 + 0.01, grow=-0.05) - inner_prism(z0 - LID_LIP_H - 0.1, z0 + 0.1, grow=-(LID_LIP_W - 0.05))
    body = body + lip
    mx, my = P.MAGNET_XY
    boss = cyl(MAG_BOSS_D, Z_MAG_BOSS_BOT, P.Z_LID_INNER + 0.01, mx, my)
    body = body + boss - cyl(P.MAGNET_D + 0.05, Z_MAGNET_BOT, z1 + 0.1, mx, my)
    base_k, lid_k = knuckle_spans()
    for y0, y1 in base_k:
        body = body - knuckle_clearance(y0, y1)
    for y0, y1 in lid_k:
        body = body + knuckle(y0, y1)
    body = body - pin_hole()
    # opening thumb lip at the +x end
    lip_face = bd.Pos(OX1 + 0.2, (OY0 + OY1) / 2) * bd.RectangleRounded(1.4, 9.0, 0.6)
    body = body + bd.extrude(bd.Pos(0, 0, z0) * lip_face, 1.0)
    body.label = "case_lid"
    body.color = P.LID_COLOR
    return body


@model
def case_magnet():
    mx, my = P.MAGNET_XY
    m = cyl(P.MAGNET_D, Z_MAGNET_BOT, Z_MAGNET_BOT + P.MAGNET_T, mx, my)
    m.label = "magnet_N52_d3x2"
    m.color = "#8e9aa6"
    return m


@model
def case_hinge_pin():
    p = bd.Pos(HINGE_X, (HINGE_Y0 + HINGE_Y1) / 2, HINGE_Z) * bd.Rot(90, 0, 0) * bd.Cylinder(HINGE_PIN_D / 2, HINGE_Y1 - HINGE_Y0)
    p.label = "hinge_pin_ss_d1"
    p.color = P.STEEL
    return p


def pogo_pin(x, y, label):
    z0 = P.Z_CASE_PCB_TOP
    barrel = cyl(P.POGO_PIN_D, z0, z0 + P.POGO_BARREL_H, x, y)
    plunger = cyl(P.POGO_PLUNGER_D, z0 + P.POGO_BARREL_H, z0 + P.POGO_WORK_H, x, y)
    pin = barrel + plunger
    pin.label = label
    pin.color = P.GOLD
    return pin


@model
def case_pcba():
    z0 = P.Z_CASE_PCB_BOT
    board = rounded_rect_prism(0, 0, P.CASE_PCB_L, P.CASE_PCB_W, P.CASE_PCB_R, z0, z0 + P.CASE_PCB_T)
    board.label = "case_pcb"
    board.color = P.FR4
    kids = [board]
    zt = z0 + P.CASE_PCB_T
    for r in case_parts():
        if r["ref"].startswith("POGO"):
            continue
        b = box_at(r["x"] - r["w"] / 2, r["y"] - r["h"] / 2, zt, r["w"], r["h"], r["z"])
        b.label = r["ref"].replace("+", "p").replace("−", "n").replace("-", "n")
        b.color = "#c9c9c9" if r["ref"] == "J1" else "#3b3b3b"
        kids.append(b)
    for i, (x, y) in enumerate(P.CASE_POGO, 1):
        kids.append(pogo_pin(x, y, f"P{i}"))
    return bd.Compound(children=kids, label="case_pcba")


@model
def case_cell():
    l, w, t = P.CASE_CELL
    y0 = (P.CASE_PCB_W - w) / 2
    x0 = P.CASE_CELL_X0
    cell = rounded_rect_prism(x0, y0, x0 + l, y0 + w, 0.8, P.CASE_FLOOR_T, P.CASE_FLOOR_T + t)
    cell.label = "cell_LP602525"
    cell.color = P.CELL_COLOR
    lm, wm, tm = P.CASE_CELL_MAX
    foam = rounded_rect_prism(x0 + 1, y0 + 1, x0 + l - 1, y0 + w - 1, 0.8, P.CASE_FLOOR_T + t, P.Z_CASE_PCB_BOT)
    foam.label = "cell_foam"
    foam.color = P.PORON_COLOR
    return bd.Compound(children=[cell, foam], label="case_cell")


def magnet_bz(px, py, pz, cx, cy, zbot, d=P.MAGNET_D, t=P.MAGNET_T, br=P.MAGNET_BR, nl=60, ns=180):
    """Bz (T) at a point (mm) from an axial cylinder magnet whose bottom face is at zbot.

    Sheet-current model (K = Br / mu0 on the side) integrated with Biot-Savart, so it holds
    off-axis, where a hall sensor sitting beside the magnet sees the field.
    """
    import numpy as np

    mu0 = 4e-7 * math.pi
    a, dz = d / 2, t / nl
    i_loop = br / mu0 * dz * 1e-3
    th = np.linspace(0, 2 * math.pi, ns, endpoint=False)
    dl = 2 * math.pi / ns * a * 1e-3
    x, y = cx + a * np.cos(th), cy + a * np.sin(th)
    bz = 0.0
    for z0 in zbot + dz * (np.arange(nl) + 0.5):
        rx, ry, rz = (px - x) * 1e-3, (py - y) * 1e-3, (pz - z0) * 1e-3
        r3 = (rx * rx + ry * ry + rz * rz) ** 1.5
        bz += float(np.sum(mu0 * i_loop / (4 * math.pi) * dl * (-np.sin(th) * ry - np.cos(th) * rx) / r3))
    return bz


def magnet_field(z, d=P.MAGNET_D, t=P.MAGNET_T, br=P.MAGNET_BR):
    """On-axis flux density (T) of an axial cylinder magnet at distance z from its face."""
    r = d / 2
    return br / 2 * ((z + t) / math.hypot(r, z + t) - z / math.hypot(r, z))


if __name__ == "__main__":
    case_base()
    case_cradle()
    case_lid()
    case_magnet()
    case_hinge_pin()
    case_pcba()
    case_cell()
