# /// script
# dependencies = ["matplotlib>=3.8", "numpy>=1.26"]
# ///
"""Draw the README figures into docs/images/ as SVG line art.

CAD drawings are projected from the build123d models with hidden-line removal, so every
line is real geometry. Diagrams use the same strokes and type.

  atrium-hero.svg       pod, isometric, internals in phantom
  atrium-exploded.svg   pod, exploded with labels
  atrium-skin.svg       skin side with the optical layout
  atrium-section.svg    section through the optical axis with the z-stack
  atrium-case.svg       charging case, lid off, pod docked
  atrium-system.svg     system architecture
  atrium-optics.svg     how the optics work

Usage: cad run docs/make_figures.py
"""

import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Polygon

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "images"
sys.path.insert(0, str(ROOT / "hardware" / "pod" / "mechanical" / "src"))
sys.path.insert(0, str(ROOT / "hardware" / "case" / "mechanical" / "src"))

import build123d as bd  # noqa: E402

from lib import params as P  # noqa: E402

PAPER = "#fbfbfd"
INK = "#1d1d1f"
GREY = "#6e6e73"
LIGHT = "#c7c7cc"
HAIR = "#d2d2d7"
BLUE = "#0071e3"
GREEN, RED, VIOLET, AMBER = "#34c759", "#ff3b30", "#af52de", "#ff9f0a"
FONT_STACK = "'Helvetica Neue', Helvetica, Arial, sans-serif"

plt.rcParams.update({
    "font.family": ["Helvetica Neue", "DejaVu Sans"],
    "svg.fonttype": "none",
    "text.color": INK,
    "savefig.facecolor": PAPER,
    "figure.facecolor": PAPER,
})


def save(fig, name):
    import os

    path = OUT / name
    fig.savefig(path, bbox_inches="tight", pad_inches=0.3)
    if os.environ.get("ATRIUM_PREVIEW"):
        fig.savefig(Path(os.environ["ATRIUM_PREVIEW"]) / name.replace(".svg", ".png"), bbox_inches="tight", pad_inches=0.3, dpi=110)
    plt.close(fig)
    svg = path.read_text()
    svg = re.sub(r"font-family:\s*'?[^;\"']+'?", f"font-family: {FONT_STACK}", svg)
    svg = re.sub(r'font-family="[^"]+"', f'font-family="{FONT_STACK}"', svg)
    path.write_text(svg)


# ------------------------------------------------------------------ projection
def polylines(edges, step=0.25):
    out = []
    for e in edges:
        n = 2 if e.geom_type == bd.GeomType.LINE else max(4, int(e.length / step))
        pts = [e.position_at(i / (n - 1)) for i in range(n)]
        out.append(np.array([(p.X, p.Y) for p in pts]))
    return out


def project(shape, eye, target, up=(0, 0, 1)):
    vis, hid = shape.project_to_viewport(eye, up, target)
    return polylines(vis), polylines(hid)


def draw(ax, lines, color=INK, lw=0.9, alpha=1.0, dashed=False, z=3):
    for pl in lines:
        ax.plot(pl[:, 0], pl[:, 1], color=color, lw=lw, alpha=alpha, solid_capstyle="round",
                ls=(0, (2.2, 1.6)) if dashed else "-", zorder=z)


def leaves(node, parent=None, path=""):
    here = (bd.Location() if parent is None else parent) * node.location
    name = f"{path}/{node.label}" if path else (node.label or "")
    if node.children:
        for c in node.children:
            yield from leaves(c, here, name)
    else:
        part = node.located(here)
        part.label = name
        yield part


def pick(tree, *keys, exclude=()):
    return [p for p in leaves(tree) if any(k in (p.label or "") for k in keys) and not any(x in (p.label or "") for x in exclude)]


def compound(parts):
    return bd.Compound(children=[p for p in parts])


def canvas(w, h):
    fig, ax = plt.subplots(figsize=(w, h), dpi=150)
    ax.set_aspect("equal")
    ax.axis("off")
    return fig, ax


def label(ax, xy, text, sub=None, xytext=None, ha="left", color=INK):
    x, y = xy
    tx, ty = xytext if xytext else (x, y)
    if xytext:
        ax.plot([x, tx], [y, ty], color=LIGHT, lw=0.6, zorder=2)
        ax.scatter([x], [y], s=6, color=INK, zorder=4, linewidths=0)
    ax.text(tx + (0.4 if ha == "left" else -0.4), ty, text, ha=ha, va="bottom", fontsize=9.5, weight="bold", color=color)
    if sub:
        ax.text(tx + (0.4 if ha == "left" else -0.4), ty - 0.15, sub, ha=ha, va="top", fontsize=8, color=GREY)


EYE_ISO = (14 + 70, 7 - 95, 3.5 + 70)
TARGET = (14, 7, 3.5)


# ------------------------------------------------------------------ CAD figures
def hero(pod):
    fig, ax = canvas(15, 7.6)
    shell = pick(pod, "pod_top_shell")[0]
    inner = {
        "cell": (pick(pod, "cell_301020"), "#8e8e93", 0.7),
        "pcb": (pick(pod, "pod_pcb"), GREEN, 0.7),
        "window": (pick(pod, "window_frame"), INK, 0.45),
        "clear": (pick(pod, "clear_"), BLUE, 0.9),
        "antenna": (pick(pod, "AE1", "antenna_riser"), AMBER, 0.7),
    }
    for parts, color, lw in inner.values():
        vis, hid = project(compound(parts), EYE_ISO, TARGET)
        draw(ax, vis + hid, color=color, lw=lw, alpha=0.55, z=2)
    vis, hid = project(shell, EYE_ISO, TARGET)
    draw(ax, vis, lw=1.5, z=4)
    draw(ax, hid, color=LIGHT, lw=0.5, dashed=True, z=1)
    ax.autoscale()
    x0, x1 = ax.get_xlim()
    y0, y1 = ax.get_ylim()
    ax.set_xlim(x0 - 6, x1 + 26)
    ax.set_ylim(y0 - 5, y1 + 6)
    xt = x1 + 4
    ax.text(xt, y1 - 1, "Atrium", fontsize=30, weight="bold", va="top")
    ax.text(xt, y1 - 6.2, "An open optical wearable.", fontsize=13, color=GREY, va="top")
    rows = (("30 × 16 × 7.0 mm", "stadium shell, flat top"), ("3 wavelengths", "530 · 655 · 940 nm"), ("2 detectors", "3.5 mm and 18.5 mm"), ("Bluetooth LE", "nRF52832"))
    for i, (a, b) in enumerate(rows):
        y = y1 - 10.5 - i * 3.0
        ax.text(xt, y, a, fontsize=10.5, weight="bold", va="top")
        ax.text(xt, y - 1.25, b, fontsize=9, color=GREY, va="top")
    for i, (name, (_, color, _)) in enumerate(inner.items()):
        lx = x0 + 2 + i * 6.2
        ax.plot([lx, lx + 1.4], [y0 - 3.2, y0 - 3.2], color=color, lw=1.2, alpha=0.8)
        ax.text(lx + 1.8, y0 - 3.2, name, fontsize=8, color=GREY, va="center")
    save(fig, "atrium-hero.svg")


def exploded(pod):
    fig, ax = canvas(15, 10)
    groups = (
        ("Top shell", "PC/ABS, 0.5 mm top, 1.2 mm edge radius", ("pod_top_shell",), (), 26.0),
        ("PORON pad", "absorbs cell swell", ("poron",), (), 19.0),
        ("Cell", "~40 mAh Li-po, PI insulated", ("cell_assembly",), (), 13.0),
        ("Board", "28 × 14 mm, 4-layer 0.6 mm", ("pod_pcba",), (), 6.5),
        ("Optical window", "black PC with clear apertures, light ribs", ("window_", "thermal_", "pogo_seal"), (), 0.0),
        ("Adhesive", "single-use, die-cut", ("adhesive", "liner"), (), -6.0),
    )
    rows = []
    for name, sub, keys, excl, dz in groups:
        parts = [bd.Pos(0, 0, dz) * p for p in pick(pod, *keys, exclude=excl)]
        vis, _ = project(compound(parts), EYE_ISO, TARGET)
        draw(ax, vis, lw=1.0 if name != "Board" else 0.7, color=INK if name not in ("Optical window",) else INK)
        if name == "Optical window":
            clear = [bd.Pos(0, 0, dz) * p for p in pick(pod, "clear_")]
            cv, _ = project(compound(clear), EYE_ISO, TARGET)
            draw(ax, cv, color=BLUE, lw=1.0, z=5)
        pts = np.vstack(vis)
        rows.append((name, sub, pts[:, 0].max(), np.median(pts[:, 1])))
    ax.autoscale()
    x0, x1 = ax.get_xlim()
    xt = x1 + 6
    rows.sort(key=lambda r: -r[3])
    placed = []
    for name, sub, xr, ym in rows:
        yt = ym if not placed else min(ym, placed[-1] - 3.6)
        placed.append(yt)
        label(ax, (xr - 2, ym), name, sub, xytext=(xt, yt))
    ax.set_xlim(x0 - 2, xt + 30)
    ax.text(x0, ax.get_ylim()[1] + 2, "Inside the pod", fontsize=20, weight="bold")
    save(fig, "atrium-exploded.svg")


def skin(pod):
    fig, ax = canvas(15, 8.2)
    eye, target = (14, 7, -100), (14, 7, 0)
    vis, hid = project(compound(pick(pod, "window_frame", "clear_", "thermal_button", "pad_J1", "pod_top_shell")), eye, target, up=(0, 1, 0))
    draw(ax, vis, lw=0.9)

    def to2d(x, y):
        v, _ = project(bd.Pos(x, y, 0) * bd.Box(0.01, 0.01, 0.01), eye, target, up=(0, 1, 0))
        pts = np.vstack(v)
        return pts[:, 0].mean(), pts[:, 1].mean()

    led, pd1, tmp, pd2 = to2d(*P.LED_XY), to2d(*P.PD1_XY), to2d(*P.TMP_XY), to2d(*P.PD2_XY)
    for r, a in ((2.6, 0.06), (1.8, 0.12), (1.15, 0.22)):
        ax.add_patch(plt.Circle(led, r, fc=GREEN, ec="none", alpha=a, zorder=1))
    for c in (pd1, pd2):
        ax.add_patch(plt.Circle(c, 1.2, fc=BLUE, ec="none", alpha=0.08, zorder=1))
    ax.add_patch(plt.Circle(tmp, 1.6, fc=AMBER, ec="none", alpha=0.1, zorder=1))
    ytop = max(np.vstack(vis)[:, 1]) + 2.2
    ybot = min(np.vstack(vis)[:, 1]) - 2.2
    label(ax, led, "Emitter", "green · red · IR", xytext=(led[0], ytop + 2.5), ha="left")
    label(ax, pd1, "Near detector", "3.4 mm window", xytext=(pd1[0], ybot - 3.5), ha="left")
    label(ax, tmp, "Skin temperature", "316L button", xytext=(tmp[0], ybot - 3.5), ha="left")
    label(ax, pd2, "Far detector", "2.8 mm window", xytext=(pd2[0], ybot - 3.5), ha="left")
    pads = [to2d(x, y) for x, y in P.POGO_PADS]
    pc = np.mean(pads, axis=0)
    left = min(pads, key=lambda q: q[0])
    label(ax, tuple(left), "Charging + debug", "5 contacts, 2.54 mm pitch", xytext=(left[0], ytop + 2.5), ha="left")
    yd = ybot - 0.8
    for x, t in ((pd1[0], "3.5"), (pd2[0], "18.5")):
        ax.annotate("", xy=(x, yd), xytext=(led[0], yd), arrowprops=dict(arrowstyle="<->", lw=0.6, color=GREY, shrinkA=0, shrinkB=0))
        ax.text((x + led[0]) / 2, yd + 0.25, f"{t} mm", ha="center", fontsize=8, color=GREY)
        yd -= 1.3
    ax.autoscale()
    x0, x1 = ax.get_xlim()
    y0, y1 = ax.get_ylim()
    ax.set_ylim(y0 - 3, y1 + 4)
    ax.text(x0, y1 + 2.5, "The side that touches skin", fontsize=20, weight="bold")
    ax.text(x0, y1 + 0.9, "Viewed from below. Each clear window is an island in black plastic, walled off by light barriers.", fontsize=10, color=GREY)
    save(fig, "atrium-skin.svg")


def section(pod):
    fig, ax = canvas(15, 5.6)
    plane = bd.Plane(origin=(0, 7.0, 0), x_dir=(1, 0, 0), z_dir=(0, 1, 0))
    big = (plane * bd.Rectangle(200, 200)).face()
    tint = {"pod_top_shell": "#f2f2f7", "window_frame": "#3a3a3c", "clear_": "#d6ecff", "pod_pcb": "#cfeed8", "cell_301020": "#e5e5ea",
            "thermal_button": "#d1d1d6", "poron": "#8e8e93", "adhesive": "#1d1d1f", "liner": "#f5efd9", "AE1": "#ffffff", "riser": "#e8f0dc"}
    for p in leaves(pod):
        cut = p & big
        if cut is None:
            continue
        fc = next((v for k, v in tint.items() if k in (p.label or "")), "#d1d1d6")
        for f in cut.faces():
            for k, w in enumerate([f.outer_wire()] + list(f.inner_wires())):
                pts = []
                for e in w.edges():
                    n = max(2, int(e.length / 0.05))
                    pts += [e.position_at(i / (n - 1)) for i in range(n)]
                ax.add_patch(Polygon([(q.X, q.Z) for q in pts], closed=True, fc=fc if k == 0 else PAPER, ec=INK, lw=0.45, zorder=3))
    stack = (
        (0, P.Z_WINDOW_TOP, "window 0.5"), (P.Z_WINDOW_TOP, P.Z_PCB_BOT, "skin parts 1.0"), (P.Z_PCB_BOT, P.Z_PCB_TOP, "board 0.6"),
        (P.Z_PCB_TOP, P.Z_CELL_BOT, "top parts 0.9 + tape"), (P.Z_CELL_BOT, P.Z_CELL_BOT + P.CELL_T, "cell 3.0"),
        (P.Z_CELL_BOT + P.CELL_T, P.Z_CAVITY_TOP, "PORON 0.45"), (P.Z_CAVITY_TOP, P.POD_H, "shell 0.5"),
    )
    for i, (a, b, t) in enumerate(stack):
        x = 30.8 + (i % 2) * 4.2
        ax.annotate("", xy=(x, a), xytext=(x, b), arrowprops=dict(arrowstyle="<->", lw=0.5, color=GREY, shrinkA=0, shrinkB=0))
        ax.text(x + 0.25, (a + b) / 2, t, fontsize=7.6, color=GREY, va="center")
    ax.annotate("", xy=(-2.2, 0), xytext=(-2.2, P.POD_H), arrowprops=dict(arrowstyle="<->", lw=0.6, color=INK, shrinkA=0, shrinkB=0))
    ax.text(-2.5, P.POD_H / 2, "7.0", ha="right", va="center", fontsize=9, weight="bold")
    for x, t in ((P.LED_XY[0], "LED"), (P.PD1_XY[0], "near"), (P.TMP_XY[0], "temp"), (P.PD2_XY[0], "far")):
        ax.plot([x, x], [-0.9, -0.35], color=LIGHT, lw=0.6)
        ax.text(x, -1.1, t, ha="center", va="top", fontsize=8, color=GREY)
    ax.plot([P.ANT_KEEPOUT_X] * 2, [-0.2, 7.2], color=AMBER, lw=0.7, ls=(0, (3, 2)))
    ax.text(P.ANT_KEEPOUT_X + 0.25, 7.25, "antenna keep-out", fontsize=7.6, color=AMBER)
    ax.set_xlim(-4, 41)
    ax.set_ylim(-2.4, 9.8)
    ax.text(-3.8, 9.2, "Section through the optical axis", fontsize=18, weight="bold")
    ax.text(-3.8, 8.3, "Every layer accounted for in 7.0 mm.", fontsize=10, color=GREY)
    save(fig, "atrium-section.svg")


def case_fig(case):
    fig, ax = canvas(15, 9)
    eye, target = (20 + 55, 13 - 85, 10 + 60), (16, 13, 12)
    solids = pick(case, "case_base", "case_cradle", "case_lid", "pod_top_shell", "hinge_pin", "case_pcb", "/P1", "/P2")
    vis, hid = project(compound(solids), eye, target)
    draw(ax, vis, lw=1.0)
    draw(ax, hid, color=LIGHT, lw=0.4, dashed=True, z=1)
    cell = pick(case, "cell_LP602525")
    cv, ch = project(compound(cell), eye, target)
    draw(ax, cv + ch, color="#8e8e93", lw=0.6, alpha=0.5, dashed=True, z=1)
    ax.autoscale()
    x0, x1 = ax.get_xlim()
    y0, y1 = ax.get_ylim()
    ax.set_xlim(x0 - 2, x1 + (x1 - x0) * 0.55)
    T = ax.transAxes
    ax.text(0.68, 0.93, "Charging case", fontsize=20, weight="bold", va="top", transform=T)
    rows = (("44 × 29 × 20 mm", "barrel hinge, magnet closure"), ("300 mAh", "several pod charges"), ("USB-C", "5 V in"), ("Two pogo pins", "live only with the lid closed"))
    for i, (a, b) in enumerate(rows):
        y = 0.82 - i * 0.1
        ax.text(0.68, y, a, fontsize=10.5, weight="bold", va="top", transform=T)
        ax.text(0.68, y - 0.035, b, fontsize=9, color=GREY, va="top", transform=T)
    save(fig, "atrium-case.svg")


# ------------------------------------------------------------------ diagrams
def tile(ax, x, y, w, h, title, sub, color):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=0.14", fc="#ffffff", ec=HAIR, lw=0.9, zorder=2))
    ax.plot([x + 0.16, x + 0.16], [y + 0.18, y + h - 0.18], color=color, lw=2.2, solid_capstyle="round", zorder=3)
    ax.text(x + w / 2 + 0.08, y + h / 2 + (0.13 if sub else 0), title, ha="center", va="center", fontsize=9.6, weight="bold", zorder=3)
    if sub:
        ax.text(x + w / 2 + 0.08, y + h / 2 - 0.19, sub, ha="center", va="center", fontsize=7.4, color=GREY, zorder=3)
    return dict(l=x, r=x + w, b=y, t=y + h, cx=x + w / 2, cy=y + h / 2)


def wire(ax, pts, text=None, at=0, both=False, color=GREY, lw=0.9, dashed=False):
    xs, ys = zip(*pts)
    ax.plot(xs, ys, color=color, lw=lw, ls=(0, (4, 3)) if dashed else "-", zorder=1.5)
    for (xa, ya), (xb, yb) in [(pts[-2], pts[-1])] + ([(pts[1], pts[0])] if both else []):
        d = np.array([xb - xa, yb - ya], float)
        d /= np.linalg.norm(d)
        n = np.array([-d[1], d[0]])
        tip = np.array([xb, yb])
        ax.add_patch(Polygon([tip, tip - 0.13 * d + 0.055 * n, tip - 0.13 * d - 0.055 * n], closed=True, fc=color, ec=color, zorder=1.6))
    if text:
        (xa, ya), (xb, yb) = pts[at], pts[at + 1]
        if abs(ya - yb) < 1e-9:
            ax.text((xa + xb) / 2, ya + 0.09, text, ha="center", va="bottom", fontsize=7.4, color=GREY)
        else:
            ax.text(xa + 0.1, (ya + yb) / 2, text, ha="left", va="center", fontsize=7.4, color=GREY)


def system():
    fig, ax = plt.subplots(figsize=(16, 10.6), dpi=150)
    ax.set_xlim(0, 16)
    ax.set_ylim(0, 10.6)
    ax.axis("off")
    S, R, Pw, St, IO = GREEN, BLUE, AMBER, VIOLET, "#8e8e93"
    ax.text(0.45, 10.15, "System architecture", fontsize=22, weight="bold", va="center")
    ax.text(0.45, 9.6, "Two boards. One radio. Every part from a standard catalogue.", fontsize=11, color=GREY, va="center")
    for x0, y0, x1, y1, t, sub in ((0.4, 3.75, 15.6, 9.1, "Pod", "28 × 14 mm · 4-layer 0.6 mm"), (0.4, 0.35, 15.6, 3.15, "Case", "40 × 26 mm · 2-layer")):
        ax.add_patch(FancyBboxPatch((x0, y0), x1 - x0, y1 - y0, boxstyle="round,pad=0,rounding_size=0.3", fc="none", ec=HAIR, lw=1.0))
        ax.text(x0 + 0.4, y1 - 0.35, t, fontsize=12.5, weight="bold", va="center")
        ax.text(x0 + 0.4 + 0.13 * len(t) + 0.35, y1 - 0.35, sub, fontsize=8.6, color=GREY, va="center")

    led = tile(ax, 0.9, 7.3, 2.35, 0.9, "SFH 7016", "green · red · IR", S)
    pd = tile(ax, 3.5, 7.3, 2.35, 0.9, "VEMD5080 × 2", "near · far", S)
    afe = tile(ax, 0.9, 5.75, 4.95, 1.0, "MAX86174A", "optical front end", S)
    soc = tile(ax, 6.65, 4.95, 2.6, 3.25, "nRF52832", "Bluetooth LE · M4F", R)
    ant = tile(ax, 10.55, 7.3, 2.35, 0.9, "Antenna", "2.4 GHz chip", R)
    fl = tile(ax, 13.2, 7.3, 2.0, 0.9, "W25Q64JW", "64 Mbit flash", St)
    imu = tile(ax, 10.55, 6.45, 2.35, 0.6, "LSM6DSV16X", "motion", S)
    tmp = tile(ax, 10.55, 5.72, 2.35, 0.6, "TMP117 · NTC", "temperature", S)
    fg = tile(ax, 10.55, 5.0, 2.35, 0.58, "MAX17048", "fuel gauge", Pw)
    pads = tile(ax, 0.9, 4.1, 2.0, 0.75, "Pogo pads", "5 contacts", IO)
    chg = tile(ax, 3.25, 4.1, 2.0, 0.75, "BQ25101", "charger", Pw)
    cell = tile(ax, 5.6, 4.1, 1.85, 0.75, "Cell", "~40 mAh", Pw)
    prot = tile(ax, 7.8, 4.1, 1.9, 0.75, "LPB1010", "protection", Pw)
    buck = tile(ax, 13.2, 4.1, 2.0, 0.75, "TPS62840", "1.8 V rail", Pw)
    wire(ax, [(2.07, afe["t"]), (2.07, led["b"])], "drive")
    wire(ax, [(4.67, pd["b"]), (4.67, afe["t"])], "signal")
    wire(ax, [(afe["r"], 6.25), (soc["l"], 6.25)], "I²C", both=True)
    wire(ax, [(soc["r"], 7.75), (ant["l"], 7.75)], "RF")
    wire(ax, [(8.6, soc["t"]), (8.6, 8.5), (fl["cx"], 8.5), (fl["cx"], fl["t"])], "SPI", 1)
    sp = 10.05
    ax.plot([sp, sp], [fg["cy"], imu["cy"]], color=GREY, lw=1.1)
    wire(ax, [(soc["r"], 6.02), (sp, 6.02)], both=True)
    for b in (imu, tmp, fg):
        wire(ax, [(sp, b["cy"]), (b["l"], b["cy"])])
    for a, b in ((pads, chg), (chg, cell), (cell, prot)):
        wire(ax, [(a["r"], 4.48), (b["l"], 4.48)])
    wire(ax, [(prot["r"], 4.48), (buck["l"], 4.48)], "VBAT")

    usb = tile(ax, 0.9, 0.75, 1.9, 1.0, "USB-C", "5 V in", IO)
    cc = tile(ax, 3.2, 0.75, 2.4, 1.0, "BQ24075", "charger · power path", Pw)
    cb = tile(ax, 6.0, 0.75, 2.0, 1.0, "LP602525", "300 mAh", Pw)
    bo = tile(ax, 8.4, 0.75, 2.0, 1.0, "TPS61099", "5 V boost", Pw)
    li = tile(ax, 10.8, 0.75, 2.0, 1.0, "SY6280", "current limit", Pw)
    pi = tile(ax, 13.2, 0.75, 2.0, 1.0, "Pogo pins", "2 × Ø2.0", IO)
    ha = tile(ax, 10.8, 2.0, 2.0, 0.58, "DRV5032", "", S)
    ax.text(ha["r"] + 0.12, ha["cy"], "lid magnet", fontsize=7.6, color=GREY, va="center")
    for a, b in ((usb, cc), (cc, cb), (cb, bo), (bo, li), (li, pi)):
        wire(ax, [(a["r"], 1.25), (b["l"], 1.25)])
    wire(ax, [(ha["cx"], ha["b"]), (ha["cx"], li["t"])])
    wire(ax, [(pi["cx"], pi["t"]), (pi["cx"], 3.45), (pads["cx"], 3.45), (pads["cx"], pads["b"])], "docked: power + ground", 1, color=BLUE, lw=1.3, dashed=True)
    for x, (c, t) in zip((10.6, 11.7, 12.85, 13.9, 15.0), ((R, "radio"), (S, "sensing"), (Pw, "power"), (St, "storage"), (IO, "contacts"))):
        ax.plot([x, x], [10.05, 10.27], color=c, lw=2.2, solid_capstyle="round")
        ax.text(x + 0.12, 10.16, t, fontsize=8.6, color=GREY, va="center")
    save(fig, "atrium-system.svg")


def optics():
    fig, ax = canvas(16, 9)
    ax.set_xlim(-3.5, 33.5)
    ax.set_ylim(-12.4, 11.6)
    ax.text(14, 10.9, "Light in. Pulse out.", ha="center", fontsize=24, weight="bold", va="center")
    ax.text(14, 9.85, "Three wavelengths, two distances. Section through the optical axis, pod on skin.", ha="center", fontsize=10.5, color=GREY, va="center")
    for y, t in ((0.0, None), (-2.2, "dermis"), (-6.5, "subcutaneous tissue")):
        ax.plot([-3.5, 33.5], [y, y], color=HAIR if y else INK, lw=0.8 if y else 1.0, ls="-" if not y else (0, (5, 4)))
    for y, t in ((-1.1, "dermis"), (-4.35, "subcutaneous tissue"), (-9.6, "deeper tissue")):
        ax.text(33.2, y, t, ha="right", va="center", fontsize=8.2, color=GREY)

    def box(x, y, w, h, lw=0.7, fc="none", r=0.0):
        if r:
            ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle=f"round,pad=0,rounding_size={r}", fc=fc, ec=INK, lw=lw, zorder=3))
        else:
            ax.add_patch(plt.Rectangle((x, y), w, h, fc=fc, ec=INK, lw=lw, zorder=3))

    box(-1.0, 0.0, 30.0, 7.0, lw=1.3, r=1.2)
    box(-0.1, 0.5, 28.2, 6.0, lw=0.5, r=0.4)
    box(0.0, 1.5, 28.0, 0.6, lw=0.6, fc="#e9f7ee")
    box(3.0, 3.05, 20.0, 3.0, lw=0.6, r=0.4)
    ax.text(13.0, 4.55, "cell", ha="center", va="center", fontsize=8.5, color=GREY, zorder=4)
    for x, w in ((2.2, 2.0), (5.7, 3.4), (20.7, 2.8)):
        box(x - w / 2, 0.0, w, 0.5, lw=0.6, fc="#e6f2ff")
    box(12.75, 0.0, 2.5, 0.5, lw=0.6, fc="#ececf0")
    for x, w in ((3.43, 0.45), (10.3, 0.8), (16.85, 0.8)):
        box(x - w / 2, 0.5, w, 1.0, lw=0.5, fc=INK)
    for x, w, h in ((2.2, 1.9, 0.55), (5.7, 4.0, 0.9), (20.7, 4.0, 0.9), (14.0, 2.0, 0.8)):
        box(x - w / 2, 1.5 - h, w, h, lw=0.5)
    ax.plot([24, 24], [0.6, 6.4], color=AMBER, lw=0.7, ls=(0, (3, 2)), zorder=3)
    ax.text(26.0, 4.2, "antenna", ha="center", va="center", fontsize=8, color=GREY)

    def path(x0, x1, depth, color, lw, alpha, skew=0.0):
        t = np.linspace(0, 1, 220)
        x = x0 + (x1 - x0) * t
        y = -depth * np.sin(np.pi * t) ** 0.8 * (1 + skew * (t - 0.5))
        ax.plot(x, y, color=color, lw=lw * 4, alpha=alpha * 0.12, solid_capstyle="round", zorder=4)
        ax.plot(x, y, color=color, lw=lw, alpha=alpha, solid_capstyle="round", zorder=5)

    for d, a in ((0.9, 1.0), (1.35, 0.8), (1.8, 0.6)):
        path(2.2, 5.7, d, GREEN, 1.4, a)
    for d, a in ((2.3, 0.8), (3.0, 0.5)):
        path(2.2, 5.7, d, RED, 1.0, a)
    for d, a in ((4.2, 0.95), (5.4, 0.8), (6.8, 0.6), (8.3, 0.4)):
        path(2.2, 20.7, d, VIOLET, 1.2, a, skew=0.12)
    for k in range(4):
        ax.add_patch(plt.Circle((14.0, 0.0), 0.4 + 0.3 * k, fc="none", ec=AMBER, lw=0.7, alpha=0.7 - 0.15 * k, zorder=4))

    def call(x, t, s, xt=None):
        xt = x if xt is None else xt
        ax.plot([x, x, xt], [1.0, 7.75, 7.75], color=LIGHT, lw=0.6, zorder=2)
        ax.text(xt, 8.5, t, ha="center", va="bottom", fontsize=10, weight="bold")
        ax.text(xt, 8.4, s, ha="center", va="top", fontsize=8, color=GREY)

    call(2.2, "Emitter", "530 · 655 · 940 nm", 1.3)
    call(5.7, "Near detector", "3.5 mm away", 7.2)
    call(14.0, "Skin temperature", "through a steel button")
    call(20.7, "Far detector", "18.5 mm away")
    ax.text(-3.2, -3.9, "Near channel\npulse · heart rate · HRV", fontsize=9.5, color="#248a3d", ha="left", va="center", linespacing=1.45)
    ax.text(11.45, -9.2, "Far channel · a longer, deeper path, for research", fontsize=9.5, color="#8944ab", ha="center", va="center")
    for i, (c, t) in enumerate(((GREEN, "Green: shallow, strongest pulse"), (RED, "Red and infrared: deeper; their ratio supports oxygen-saturation research"), (VIOLET, "Infrared to the far detector"))):
        y = -10.75 - 0.6 * i
        ax.plot([-3.0, -2.0], [y, y], color=c, lw=1.6)
        ax.text(-1.7, y, t, va="center", fontsize=8.6)
    ax.text(33.2, -12.2, "Illustrative light paths. For wellness and research, not medical use.", ha="right", fontsize=7.8, color=GREY)
    save(fig, "atrium-optics.svg")


if __name__ == "__main__":
    from case_assembly import case_assembly_open
    from pod_assembly import pod_assembly, pod_assembly_docked

    OUT.mkdir(parents=True, exist_ok=True)
    docked = pod_assembly_docked().shape
    hero(docked)
    exploded(pod_assembly().shape)
    skin(docked)
    section(pod_assembly().shape)
    case_fig(case_assembly_open().shape)
    system()
    optics()
    print("wrote", OUT)
