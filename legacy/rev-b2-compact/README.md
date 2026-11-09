# Rev B.2 compact (superseded)

The step between Rev B.1 and EVT: same radio, one optical module (MAXM86161), a cell of at most 30 mAh and two charging
pads. Most of the work was form: how small and how flat the pod could be around those parts.

| Folder | Contents |
|---|---|
| `concept-sheets/` | The form families drawn around the B.2 insides: Tile, Wedge and Slate; Ingot, Sail and Float; Facet, Bow and Blade; E Slim; Disc against Sail; Blade and Sail product pages |
| `pill/` | Pill, 17.0 × 11.0 × 4.73 mm: flat top, crisp chamfer, a small round dish. STEP, STL and 3MF of shell and base |
| `pill-s/` | Pill S, 24.6 × 11.0 × 5.0 mm, built round a 9 × 9 × 4 mm cell. Shell and base CAD and the edge, section and finish studies in `renders/` |
| `board-outlines/` | Board outlines (DXF) for Blade, Sail, Pill and Pill S, and the Pill S skin-patch outline |

**Why it was left:** the single-board layouts needed more area than these forms allowed once real pads and clearances
were placed, and the thin cells the smallest forms assumed were not in stock. EVT keeps the B.2 electronics but splits
them into two rigid islands joined by a flex under a centred cell.
