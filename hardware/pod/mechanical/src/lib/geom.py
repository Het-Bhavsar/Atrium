"""Shared geometry helpers and placement readers."""

import csv

import build123d as bd

from lib import params as P


def stadium(length, width, cx=P.PCB_CX, cy=P.PCB_CY):
    """A stadium face of overall length x width, centred at (cx, cy)."""
    return bd.Pos(cx, cy) * bd.SlotOverall(length, width)


def stadium_prism(length, width, z0, z1, cx=P.PCB_CX, cy=P.PCB_CY):
    return bd.extrude(bd.Pos(0, 0, z0) * stadium(length, width, cx, cy), z1 - z0)


def union(shapes):
    out = shapes[0]
    for s in shapes[1:]:
        out = out + s
    return out


def box_at(x0, y0, z0, sx, sy, sz):
    """Box from its lower corner."""
    return bd.Pos(x0 + sx / 2, y0 + sy / 2, z0 + sz / 2) * bd.Box(sx, sy, sz)


def rounded_square(size, r, x, y):
    return bd.Pos(x, y) * bd.RectangleRounded(size, size, r)


def cyl(d, z0, z1, x, y):
    return bd.Pos(x, y, (z0 + z1) / 2) * bd.Cylinder(d / 2, z1 - z0)


def rounded_rect_prism(x0, y0, x1, y1, r, z0, z1):
    sk = bd.Pos((x0 + x1) / 2, (y0 + y1) / 2) * bd.RectangleRounded(x1 - x0, y1 - y0, r)
    return bd.extrude(bd.Pos(0, 0, z0) * sk, z1 - z0)


def read_placement(path, overrides_xy=None, overrides_size=None, overrides_h=None):
    """Placement rows as dicts with floats; applies Rev B.1 overrides."""
    overrides_xy = overrides_xy or {}
    overrides_size = overrides_size or {}
    overrides_h = overrides_h or {}
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            ref = r["ref"]
            x, y = overrides_xy.get(ref, (float(r["x_mm"]), float(r["y_mm"])))
            w, h = overrides_size.get(ref, (float(r["w_mm"]), float(r["h_mm"])))
            rows.append(
                dict(
                    ref=ref,
                    side=r["side"],
                    x=x,
                    y=y,
                    w=w,
                    h=h,
                    z=overrides_h.get(ref, float(r["max_height_mm"])),
                    label=r["label"],
                )
            )
    return rows


def pod_parts():
    return read_placement(
        P.POD_PLACEMENT_CSV,
        P.PART_XY_OVERRIDES,
        P.PART_SIZE_OVERRIDES,
        P.PART_HEIGHT_OVERRIDES,
    )


def case_parts():
    return read_placement(P.CASE_PLACEMENT_CSV)


def part_row(rows, ref):
    for r in rows:
        if r["ref"] == ref:
            return r
    raise KeyError(ref)
