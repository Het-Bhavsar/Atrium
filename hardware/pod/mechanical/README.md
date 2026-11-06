# Pod mechanical

Parametric build123d models of the pod. Every parameter is in [`src/lib/params.py`](src/lib/params.py).

- **Pod frame:** origin at the PCB bounding-box lower-left, +x toward the antenna, z = 0 at the window's skin face.
- **DXFs:** they use the shell frame, which is the pod frame + (1, 1).

```
cd src
cad run pod_assembly.py          # pod, and pod as docked (no adhesive)
cad run checks/interference.py   # pairwise overlap, expect 0
cad run checks/stackup.py        # z-stack, 10 % cell swell, skin clearances, metal keep-out
cad run checks/design_rules.py   # visible form rules
cad run exports.py               # STEP/ STL/ 3MF/ DXF/
```

## Parts

| Part | Model | Material |
|---|---|---|
| Top shell | `pod_shell.py::pod_top_shell` | PC or PC/ABS; 0.5 mm top, 0.9 mm straight wall, 1.2 mm top edge round, 0.8 mm skin edge round |
| Window frame | `pod_window.py::pod_window_frame` | black PC, first shot; perimeter rim, three light-barrier ribs, pogo sealing bar |
| Window apertures | `pod_window_clear` | clear PC, second shot; LED Ø2.0, near 3.4 □, far 2.8 □ |
| Thermal button | `thermal_button` | 316L, Ø2.45 × 0.5 with a retaining flange |
| Gap pad, pogo seal | `thermal_gap_pad`, `pogo_gasket` | 0.3 mm gap filler; black silicone 0.15 mm (`POGO_SEAL`: none / gasket / glue) |
| PCBA envelope | `pod_internals.py::pod_pcba` | boxes from `pcb/placement/pod_placement.csv` (antenna lifted 2.5 mm; `pod_pcba_antenna_flat` keeps it on the board) |
| Cell, PORON, adhesive | `pod_cell`, `pod_poron`, `pod_adhesive`, `pod_liner` | 301020 cell with PI tape; 0.5 mm PORON; 0.2 mm medical tape with a liner tab |
| Foam adhesive options | `pod_adhesive_foam05`, `pod_adhesive_foam08` | 0.5 and 0.8 mm black medical foam, same die-cut, for the wear test |
| Comfort dummy | `pod_shell.py::pod_comfort_dummy` | outer envelope as one solid with a ballast pocket, for SLA printing |

## Z-stack (mm, skin up)

| Layer | Thickness |
|---|---|
| Window | 0.5 |
| Skin-side parts | 1.0 |
| PCB | 0.6 |
| Top parts | 0.9 |
| PI tape | 0.05 |
| Cell | 3.0 |
| PORON gap | 0.45 |
| Top shell | 0.5 |
| **Total** | **7.0** |

The adhesive (0.2) sits outside this, below the window.

## Comfort

The pod sits on the side of the head, which is curved; the face is flat and rigid. Over the 30 mm length, a flat face leaves a gap at the ends or the middle:

| Skin curvature radius [ASSUMED] | Gap |
|---|---|
| 200 mm | 0.56 mm |
| 150 mm | 0.75 mm |
| 100 mm | 1.13 mm |
| 80 mm | 1.42 mm |

A 0.2 mm tape can't absorb that, so the design does three things:

1. **Skin edge** rounded 0.8 mm, so there's no pressure line. The skirt is the full 0.9 mm wall, and the window plate is flush with its rim.
2. **Patch** covers only the flat of the face (0.9 mm inside the outline), so no adhesive hangs under the round.
3. **Compliant pad option:** 0.5 and 0.8 mm foam versions of the die-cut.

**Mass:** 3.8 g estimated without the patch; the cell is 2.0 g of it (`checks/stackup.py`).

**Wear test, before shell tooling:**
- Print `pod_comfort_dummy` in SLA resin (about 2.7 g).
- Fill the pocket with steel shot in epoxy to the reported pod mass (about 1.0 g, 40 % of the pocket) and seal it flush.
- Ten people wear it for one day and one night with each pad: 0.2 tape, 0.5 foam, 0.8 foam.
- Record a 0–10 comfort score, edge lift (feeler gauge at both ends), any skin marks, and whether it stays on overnight.
- Run it alongside the 7-day adhesive patch test.

## Print notes

The 0.5 mm skins and 0.45 to 0.8 mm ribs need resin (SLA) or a 0.2 mm FDM nozzle.
