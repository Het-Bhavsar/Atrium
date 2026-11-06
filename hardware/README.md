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
| Stack-up | 4-layer HDI JLCH04061HN1-1078, 0.61 ± 0.10 mm, ENIG, laser microvias, via-in-pad under the WLCSP parts |
| Layers | L1 parts and RF; L2 solid ground; L3 detector inputs (guarded), I²C, power; L4 skin side (optics, temperature, pogo pads) |
| Antenna keep-out | x ≥ 24.0 mm on every layer; no metal in the shell, cell or foam |
| Parts | ≤ 0.83 mm tall under the cell (U3 is the tallest); ≤ 0.9 mm elsewhere on top; ≤ 1.0 mm on the skin side |

## Case board

40 × 26 mm rounded rectangle (R3), 2-layer 1.0 mm FR-4. Charge path: USB-C, BQ24073, LP602525 300 mAh cell, TPS61099 5 V boost, SY6280 current limit, two pogo pins. A DRV5032 hall sensor and the lid magnet switch the pins off when the lid is open, and a TLV803 supervisor holds the boost off below 2.93 V.

## Open items (EVT)

1. **USB-C vs docked pod: resolved.**
   - The pod and the pogo pins now sit 1.8 mm further in y (pins at y 20.3), which leaves 0.50 mm to J1 (0.35 mm with the pod's float in its pocket).
   - The hall sensor U4 moves to (36.6, 22.4) and the lid magnet to (37.55, 23.55), 1.5 mm outboard, so its pocket clears the pod pocket. |Bz| at the die is 88 mT with the lid closed.
   - Keep-outs for the case layout: `case/mechanical/DXF/atrium_case_pcb_keepouts_revB1-1.dxf`.
2. **Light barrier between the LED and the near detector.** Only 0.55 mm separates the two parts, so the rib above the window is 0.45 mm. That is thin for PC moulding. Fix one of two ways:
   - a black dispensed dam or gasket;
   - the near detector at x ≥ 6.0.
3. **Cell envelope.**
   - The supplier sheet allows 20.5 × 11.0 × 3.5 mm.
   - With the 0.83 mm under-cell rule and 0.4 mm PORON, a 10 % swell of the nominal 3.0 mm cell fits across the full PCB range: +0.13 mm at a 0.61 board, +0.03 mm at 0.71.
   - It fails under worst-case tolerances; that needs 7.34 mm. A 3.5 mm sheet-max cell needs 7.42 mm.
   - Fix one of two ways: get a drawing with a thickness tolerance, or go to a 7.4 mm height.
4. **Pogo pin data.** Barrel height, plunger diameter and stroke for C2826546 are [VERIFY].
5. **Raised antenna.** The 2.5 mm interposer is modelled mechanically; its feed and ground are still to design.

The full check output is in `pod/mechanical/renders/stackup_report.txt` and `case/mechanical/renders/case_fit_report.txt`.
