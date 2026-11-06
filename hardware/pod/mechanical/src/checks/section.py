# /// script
# dependencies = ["matplotlib>=3.8"]
# ///
"""Dimensioned section through the optical axis y = 7.0 (pod frame).

Slices every leaf solid of the pod assembly at y = SECTION_Y, fills each cut face in
its part colour, and dimensions the z-stack and the aperture/rib positions. Writes
renders/pod_section_y7.png.

Usage: cad run checks/section.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build123d as bd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon

from checks.interference import world_parts
from lib import params as P
from pod_assembly import pod_assembly
from pod_window import rib_led_pd1_span

SECTION_Y = 7.0
OUT = Path(__file__).resolve().parents[2] / "renders"


def face_loops(face, step=0.05):
    loops = []
    for wire in [face.outer_wire()] + list(face.inner_wires()):
        pts = []
        for e in wire.edges():
            n = max(2, int(e.length / step) + 1)
            pts += [e.position_at(i / (n - 1)) for i in range(n)]
        loops.append([(p.X, p.Z) for p in pts])
    return loops


def section_faces(part, y):
    plane = bd.Plane(origin=(0, y, 0), x_dir=(1, 0, 0), z_dir=(0, 1, 0))
    face = (plane * bd.Rectangle(200, 200)).face()
    cut = part & face
    return list(cut.faces()) if cut is not None else []


def main():
    tree = pod_assembly().shape
    fig, ax = plt.subplots(figsize=(16, 5.2), dpi=150)
    labels_done = set()
    for part in world_parts(tree):
        if part.volume <= 0:
            continue
        color = tuple(part.color)[:3] if part.color is not None else (0.5, 0.5, 0.5)
        for f in section_faces(part, SECTION_Y):
            for k, loop in enumerate(face_loops(f)):
                ax.add_patch(Polygon(loop, closed=True, facecolor=color if k == 0 else "white", edgecolor="k", linewidth=0.25))

    def zdim(x, z0, z1, text, side=1):
        ax.annotate("", xy=(x, z0), xytext=(x, z1), arrowprops=dict(arrowstyle="<->", lw=0.6))
        ax.text(x + 0.15 * side, (z0 + z1) / 2, text, fontsize=6.5, va="center", ha="left" if side > 0 else "right")

    zs = [
        (-P.ADHESIVE_T, 0, f"adhesive {P.ADHESIVE_T}"),
        (0, P.Z_WINDOW_TOP, f"window {P.WINDOW_T}"),
        (P.Z_WINDOW_TOP, P.Z_PCB_BOT, f"skin parts {P.SKIN_ZONE}"),
        (P.Z_PCB_BOT, P.Z_PCB_TOP, f"PCB {P.PCB_T}"),
        (P.Z_PCB_TOP, P.Z_PCB_TOP + P.TOP_ZONE, f"top parts {P.TOP_ZONE}"),
        (P.Z_PCB_TOP + P.TOP_ZONE, P.Z_CELL_BOT, f"PI tape {P.CELL_INSULATOR_T}"),
        (P.Z_CELL_BOT, P.Z_CELL_BOT + P.CELL_T, f"cell {P.CELL_T}"),
        (P.Z_CELL_BOT + P.CELL_T, P.Z_CAVITY_TOP, f"PORON gap {P.Z_CAVITY_TOP - P.Z_CELL_BOT - P.CELL_T:.2f}"),
        (P.Z_CAVITY_TOP, P.POD_H, f"top shell {P.SHELL_TOP_T}"),
    ]
    for i, (z0, z1, t) in enumerate(zs):
        zdim(30.2 + (i % 2) * 2.6, z0, z1, t)
    zdim(-2.2, 0, P.POD_H, f"{P.POD_H:.1f} outer", side=-1)

    xs = [
        ("LED Ø2.0", P.LED_XY[0]),
        ("PD1 3.4□", P.PD1_XY[0]),
        ("TMP Ø2.5 + 316L", P.TMP_XY[0]),
        ("PD2 2.8□", P.PD2_XY[0]),
    ]
    for t, x in xs:
        ax.plot([x, x], [-0.9, -0.3], color="k", lw=0.5)
        ax.text(x, -1.05, f"{t}\nx {x}", fontsize=6.5, ha="center", va="top")
    rx0, rx1 = rib_led_pd1_span()
    ribs = [("rib", (rx0 + rx1) / 2, rx1 - rx0), ("rib", P.RIB_PD1_TMP_X, P.RIB_W), ("rib", P.RIB_TMP_PD2_X, P.RIB_W)]
    for t, x, w in ribs:
        ax.text(x, 0.55, f"{t} {w:.2f}", fontsize=5.0, ha="center", va="bottom", rotation=90, color="white")
    ax.axvline(P.ANT_KEEPOUT_X, color="#c0392b", lw=0.8, ls="--")
    ax.text(P.ANT_KEEPOUT_X + 0.1, 6.85, "antenna keep-out x ≥ 24\n(no metal in shell/foam)", fontsize=6, color="#c0392b", va="top")
    ax.set_xlim(-3.5, 34.5)
    ax.set_ylim(-2.4, 7.6)
    ax.set_aspect("equal")
    ax.set_xlabel("x (mm, pod PCB frame)")
    ax.set_ylabel("z (mm, skin face = 0)")
    ax.set_title(f"Atrium pod Rev B.1: section at y = {SECTION_Y} (optical axis), antenna lift {P.ANT_LIFT} mm", fontsize=9)
    ax.grid(True, lw=0.2, alpha=0.5)
    OUT.mkdir(exist_ok=True)
    fig.tight_layout()
    fig.savefig(OUT / "pod_section_y7.png")
    print("wrote", OUT / "pod_section_y7.png")


if __name__ == "__main__":
    main()
