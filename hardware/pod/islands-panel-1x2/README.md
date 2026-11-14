# Pod islands panel, 1 × 2 (small EVT run)

The same panel as [`../islands-panel/`](../islands-panel/) cut down to two sets: 2 radio islands and 2 power islands,
so an assembled panel gives 2 pods. It exists to keep a first EVT build affordable: JLC's HDI minimum is 3 boards and
its PCBA minimum is 2, so the 4 × 5 panel means at least 40 assembled pods.

| Item | Value |
|---|---|
| Panel | 26.0 × 35.6 mm, the same 5 mm frame, 3 mm tabs and 0.50 mm mouse bites as the 4 × 5 panel |
| Contents | Sets R11 + P11 and R21 + P21, at the same coordinates as in the 4 × 5 panel |
| Stack-up | JLC06081H-2313, 0.82 mm, 6-layer HDI 1+4+1 |
| Fiducials and tooling | 3 fiducials per side, 4 × 1.152 mm NPTH tooling holes |
| Placements | 104 per panel (2 pods), 31 BOM lines |

The panel was made by copying the 4 × 5 panel PCB in EasyEDA, deleting the other 18 sets and drawing the 1 × 2 frame
with the same builder. EasyEDA DRC: 0 errors (proven live with a planted short). Inside the frame opening, all six
copper layers, both paste layers, both silkscreens and every drill hit are identical to the 4 × 5 panel's Gerbers;
solder mask differs only where the 4 × 5 panel's next row of tabs begins. The paste layers carry the same fine-pitch
apertures (U1 0.21 mm, U2 / U13 0.20 mm, U10 0.22 mm, U12 exposed pad 0.38 mm).

## Files

| File | What it is |
|---|---|
| `Pod_Islands_Panel_1x2.PcbDoc` | Panel layout (Altium format, exported from EasyEDA Pro) |
| `Pod_Islands_Panel_1x2_gerber.zip` | Gerbers, drills and the flying-probe file, with the fine-pitch paste apertures |
| `Pod_Islands_Panel_1x2_BOM.csv`, `Pod_Islands_Panel_1x2_CPL.csv` | Assembly files in JLC format: the 4 × 5 panel's rows for these four islands, checked against a fresh export (0 position differences) |

## Order notes

The order notes of the 4 × 5 panel apply (stencil, depanelling by cutting, X-ray, support, bottom side first, CPL
preview). In addition:

1. In JLC's BOM step choose **"Complete File"**: the BOM and CPL already list every island on the panel. "Single
   Piece" multiplies every quantity by the panel count.
2. The panel is under 70 mm, so let JLC add the edge rails for Standard PCBA.
3. JLC splits the L1–L5 and L2–L6 skip vias into stacked vias (no extra fee), as expected in O-32.
4. If the MAXM86161A (C54021201) is out of stock, the MAXM86161 (C3178309) fits the same land pattern and runs the same
   firmware (same register map and PART_ID); its green LED is dimmer (O-43).
