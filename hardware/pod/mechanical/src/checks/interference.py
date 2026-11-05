"""Pairwise interference over an assembly: every pair of leaf solids, exact overlap.

Usage: cad run checks/interference.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build123d as bd
from cad.geometry import overlap_volume

# Pairs that overlap on purpose: (label_a, label_b) in either order.
INTENDED = set()


def world_parts(node, parent=None):
    here = (bd.Location() if parent is None else parent) * node.location
    if node.children:
        for child in node.children:
            yield from world_parts(child, here)
    else:
        yield node.located(here)


def boxes_meet(a, b, tol=1e-6):
    A, B = a.bounding_box(optimal=False), b.bounding_box(optimal=False)
    return not (
        A.max.X < B.min.X - tol or B.max.X < A.min.X - tol
        or A.max.Y < B.min.Y - tol or B.max.Y < A.min.Y - tol
        or A.max.Z < B.min.Z - tol or B.max.Z < A.min.Z - tol
    )


def interferences(tree, min_volume=1e-4):
    parts = [p for p in world_parts(tree) if p.volume > 0]
    hits = []
    for i, a in enumerate(parts):
        for b in parts[i + 1:]:
            if (a.label, b.label) in INTENDED or (b.label, a.label) in INTENDED:
                continue
            if not boxes_meet(a, b):
                continue
            v = overlap_volume(a, b)
            if v > min_volume:
                hits.append((a.label, b.label, v))
    return parts, hits


def report(name, tree):
    parts, hits = interferences(tree)
    print(f"{name}: {len(parts)} solids, {len(hits)} interfering pairs")
    for a, b, v in sorted(hits, key=lambda h: -h[2]):
        print(f"  {a:28s} x {b:28s} {v:8.4f} mm3")
    return hits


if __name__ == "__main__":
    from pod_assembly import pod_assembly
    report("pod_assembly", pod_assembly().shape)
