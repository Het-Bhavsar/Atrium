# Pod islands panel

One 6-layer panel carries both pod islands, so a build needs one PCB order, one stencil per side and one assembly setup.

| Item | Value |
|---|---|
| Panel | 45.0 × 47.4 mm, 5 mm frame, 3 mm bar between the columns |
| Contents | 2 columns × 3 rows of sets: 6 radio islands and 6 power islands (6 pods) |
| Stack-up | JLC06081H-2313, 0.82 mm, the same as the single boards |
| Tabs | Each pod is held at both ends: two radio tip tabs (inside the antenna's copper-free zone) and one power tab at 133°; the two islands of a set are joined by two corner tabs. Nearest pad to any tab end: 0.40 mm |
| Break-off | Mouse bites: two 0.50 mm NPTH holes per tab end at 0.65 mm pitch, centres 0.30 mm outside the board edge (84 per panel) |
| Copper at the tabs | Pour kept off every tab body and 0.2 mm back from each joint, the same pull-back as every board edge |
| Fiducials and tooling | 3 fiducials per side (1.0 mm copper, 2.0 mm mask opening), 4 × 1.152 mm NPTH tooling holes |
| Placements | 312 per panel (174 top, 138 bottom), 30 BOM lines |
| Nets | Each island has its own net names (`GND_R11`, `VSYS_P32`, …), so the flying-probe netlist keeps the islands apart |

## Files

| File | What it is |
|---|---|
| `Pod_Islands_Panel.PcbDoc` | Panel layout (Altium format, exported from EasyEDA Pro) |
| `Pod_Islands_Panel_gerber.zip` | Gerbers, drills and the flying-probe file. Paste layers carry the enlarged fine-pitch apertures |
| `Pod_Islands_Panel_BOM.csv`, `Pod_Islands_Panel_CPL.csv` | Assembly files in JLC format; designators carry the set (`U1_R11` = radio, row 1, column 1) |
| `Pod_Islands_Panel_render.png` | Top and bottom with every placement marked |

The panel was checked against both source boards part by part and, as Gerbers, layer by layer: all six copper layers,
both masks and both paste layers match an independent merged-Gerber build, and the 1,366 drill hits match. EasyEDA DRC
on the panel: 0 errors.

## Order notes

1. Customer panel with two different designs. Confirm the "different designs" fee and HDI acceptance with the fab.
2. Stencil: one per side, 0.08 mm electropolished foil, on a frame. Ask the factory not to resize apertures.
3. Depanel by cutting the tabs (blade or router), never by snapping: WLCSP and DSBGA parts sit a few millimetres from
   the tabs on a 0.82 mm board. Sand the nubs flush; the islands sit in a 0.15 mm wall clearance.
4. X-ray U1 (WLCSP) and U2, U10, U13 (DSBGA) on the first panel.
5. Support the panel under the frame and bars on both passes (worst island deflection about 64 µm at a 2 N nozzle force).
6. Bottom side first.
7. Check the orientation of the WLCSP, DSBGA, LGA and DFN parts in the fab's CPL preview before paying.
