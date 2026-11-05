# Hardware

Revision B.1, engineering validation (EVT). All dimensions are in mm.

| Folder | Contents |
|---|---|
| `pod/schematic`, `case/schematic` | Pin-level netlists (CSV); schematic PDFs from EasyEDA Pro |
| `pod/pcb`, `case/pcb` | Placement (`placement/*.csv`); layout files once routed |
| `pod/bom`, `case/bom` | BOM with LCSC part numbers and JLC part type |
| `pod/mechanical`, `case/mechanical` | Parametric build123d CAD, checks, and STEP / STL / 3MF / DXF exports |
| `pod/production`, `case/production` | Gerbers, pick-and-place and assembly drawings, after layout |
| `simulation` | Power, signal, thermal, RF and SPICE models behind the numbers |

## Pod board

| | |
|---|---|
| Outline | Stadium 28 × 14 mm, end radius 7.0; origin at the bounding-box lower-left, +x toward the antenna |
| Stack-up | 4 layers, 0.6 mm, ENIG, laser microvias, via-in-pad under the WLCSP parts |
| Layers | L1 parts and RF; L2 solid ground; L3 detector inputs (guarded), I²C, power; L4 skin side (optics, temperature, pogo pads) |
| Antenna keep-out | x ≥ 24.0 mm on every layer; no metal in the shell, cell or foam |
| Parts | ≤ 0.9 mm tall under the cell; ≤ 1.0 mm on the skin side |

## Case board

40 × 26 mm rounded rectangle (R3), 2-layer 1.0 mm FR-4. Charge path: USB-C, BQ24075, LP602525 300 mAh cell, TPS61099 5 V boost, SY6280 current limit, two pogo pins. A DRV5032 hall sensor and the lid magnet switch the pins off when the lid is open.

## Open items (EVT)

1. **USB-C vs docked pod.** The case receptacle J1 (3.3 mm tall, rear edge at y 6.8) overlaps the docked pod by 1.3 mm. Fix one of two ways:
   - move the pogo pins and the pod +1.6 mm in y;
   - use a mid-mount USB-C about 1.2 mm lower.
2. **Light barrier between the LED and the near detector.** Only 0.55 mm separates the two parts, so the rib above the window is 0.45 mm. That is thin for PC moulding. Fix one of two ways:
   - a black dispensed dam or gasket;
   - the near detector at x ≥ 6.0.
3. **Cell envelope.** The supplier sheet allows 20.5 × 11.0 × 3.5 mm. A 10 % swell of the nominal 3.0 mm cell fits at nominal (+0.05 to +0.12 mm), but not under worst-case tolerances. Fix before freezing the height, one of two ways:
   - get a drawing with a thickness tolerance;
   - go to a 7.4 mm height.
4. **Pogo pin data.** Barrel height, plunger diameter and stroke for C2826546 are [VERIFY].
5. **Raised antenna.** The 2.5 mm interposer is modelled mechanically; its feed and ground are still to design.

The full check output is in `pod/mechanical/renders/stackup_report.txt` and `case/mechanical/renders/case_fit_report.txt`.
