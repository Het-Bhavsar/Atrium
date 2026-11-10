# PCB rules: radio and power islands

Two separate rigid boards, ordered from JLC on one panel. Outlines: `hardware/design-data/outline/board_radio_edge_cuts.dxf`
and `board_power_edge_cuts.dxf`. Placement intent and keep-outs are in `atrium_outlines.dxf` (top view; bottom
features drawn as seen from the top, as in KiCad/EasyEDA).

## Fab

| Item | Value |
|---|---|
| Layers / thickness | **6, 0.82 mm, JLC06081H-2313** (decided 2026-11-08); controlled impedance. JLC now names this prepreg 3313 (same thickness and εr as 2313, per JLC's impedance-calculator guide), so the order may show JLC06081H-3313. The stack is set in each board's EasyEDA Layer Manager; the Altium export does not carry it. JLC standard 4/6-layer starts at 0.8 mm; HDI also offers 0.6 mm 4-layer, but the 4-layer routing under U1 did not close |
| Stackup | L1 signal + RF / PP 2313 0.0964 / L2 solid GND / core 0.15 / L3 signal / PP 2x2313 / L4 signal + power / core 0.15 / L5 solid GND / PP 2313 0.0964 / L6 signal + bottom parts (inner Cu 0.5 oz, outer 1 oz, Tg170) |
| Min trace/space | Design at 0.10/0.10 mm; 0.09 only in the WLCSP fan-out |
| Vias | **HDI 1-step 1+4+1**: laser microvias L1–L2 and L5–L6 0.10 hole / 0.25 pad (aspect 0.96 on 0.0964 prepreg, JLC max 1:1), copper filled, in the ball pads of the 0.4 mm WLCSP-50 and DSBGA-20; buried L2–L5 0.15 / 0.30; stacked skip L1–L5 / L2–L6 0.10 / 0.25 (laser on buried); through 0.15 / 0.25 where both sides are clear. EasyEDA blind-via rules `1-2`, `5-6`, `2-5`, `1-5`, `2-6`. Quote 2026-11-08 (5 pcs, 7 × 10 mm, ENIG, filled & capped): about $222 + buried-via and lamination fees (manual quote), 12–13 days |
| Finish | ENIG |
| Copper to edge | >= 0.20 mm; blind and skip vias: hole edge >= 0.35 mm from the routed edge (JLC HDI rules) |
| Why HDI | The radio bottom carries the MAXM86161 pad columns and the flex tongue directly under U1: 13 of U1's 15 inner-ball through vias would land on other-net bottom pads (DRC 2026-11-07; a placement scan found one legal through-via spot for U1 exists, but it leaves no room for the RF match before the antenna zone). Microvias stop at L2, so nothing under U1 reaches the bottom. The same holds for U10 (bottom) under U11 (top) on the power island. |
| Assembly | JLC **Standard** PCBA, both sides, X-ray for U1/U10. Panelised with rails and fiducials; mouse-bites, never V-score (curved outlines, 0201 parts near the edge). |

## Radio island (x 13.86 to 20.3, y 0.6 to 10.4: a rectangle to x 15.40 closed by an R 4.9 semicircle)

Placement: all 64 parts were placed with real pads and clearances before routing. Use it as the starting placement; keep its side assignments unless the tool still passes after a change.


- **Top**: U1 nRF52832 (see placement file), Y1 NX1612SA 1.88 mm from XC1/XC2, TH4 (top NTC) beside R11, L1/L2 DC/DC per Nordic, U2 LDO, RF match C3/L3, pi pads C40/Z41/C42, ANT1 at the tip, C30–C32 for the MAXM86161 directly above its pads, R1–R3, R10/R11, R40/R41/C12.
- **Bottom**: U3 MAXM86161 (x 15.66–18.56, y 3.35–7.65), TH1 over the base boss (0.10 mm gel gap), jumper tongue at the inner edge (coordinates in the outline DXF). Small passives the placement moved here are allowed (all <= 0.80 mm).
- **Antenna zone** (as built, checked on the Gerbers 2026-11-10): x >= 18.76 mm. L2-L6 carry no copper there (no pour, trace, via or plane); L1 carries only ANT1's pads and its feed; ANT1 pin 1 (far end) is a bare pad, not grounded (2026-11-10). Ground sits about 0.25 mm from the antenna body, about half of Kinghelm's minimum clearance (their layouts are 5 x 3 to 5.5 x 4 mm), so the pi match is tuned on the head (open items O-7, O-13).
- **RF feed**: nRF ANT -> C3 1.0 pF shunt (ground via beside it) -> L3 3.3 nH -> 50 ohm CPWG on L1 over solid L2 -> pi (C42 shunt / Z41 series / C40 shunt) -> ANT1. Feed under 3 mm long. **Arc bends, radius >= 3x width.** No vias. Via fence both sides at <= 1 mm. Width **0.124 mm with a 0.127 mm gap** to the L1 ground, L2 reference (JLC calculator, JLC06081H-2313, coplanar single-ended with mask, 2026-11-08; the field solver in hardware/simulation/cpwg_fd.md gives 53 ohm with its assumed mask). GND vias along it are L1–L2 microvias.
- **Crystal**: no vias in XC traces, ground ring, nothing switching under it, caps grounded next to the crystal ground pad.
- **DC/DC loop** (DCC -> L1 -> L2 -> DEC4 -> C9 -> GND): smallest possible area, kept away from the RF feed and Y1.
- **MAXM86161**: own vias for PGND, GND_DIG and GND_ANA (L4–L3 microvia + buried L3–L2). VLED cap C30 with its own via. No copper pour on L1 under the optical window region on the bottom (only the module's pads).

## Power island (x 0.6 to 4.16)

- **Top**: U11 LSM6DSOX (see placement file), TH5 next to the cell, CELL+ / CELL- tab pads, C50, C52, C53, C56, C60, C61, R50.
- **Bottom**: U10 BQ21061 (DSBGA, 0.5 mm), U13 TPS7A0218 + C62/C63 (IMU supply), jumper pads (two rows at the inner edge), U12 AP6683 + R51/C55, C51 22 µF 0402 (height <= 0.65 mm), D10, C60/C61.
- **Clamp bands** (`geometry.CLAMPS`, mechanical): power 114° / 246°, radio ±60° / ±86°, 0.35 × 1.0 mm at the edge, keep-out on both sides.
- **Bottom part height <= 0.80 mm** (0.85 mm to the base).
- Battery current path CELL- -> AP6683 -> GND short and wide. The IMU sits on L1 over solid L2 with no switching nearby.

## Routing rules (EMC)

Stack references: **L1 and L3 return on L2; L4 and L6 return on L5** (L3 sits 0.15 mm over L2 and ~0.19 mm under
L4, L4 0.15 mm over L5). L2 and L5 are the only planes and both are GND.

| # | Rule | Why |
|---|---|---|
| R1 | **No signal or power copper on L2 or L5** (one documented exception: DEC1's 1.3 mm L2 hop in the U1 corner, where U1 column 7 sits 0.34 mm from the edge and the VDD column owns L3; Nordic's CIAA fans out on inner 1 the same way). Both are unbroken GND pours over the whole island (minus the antenna zone). | A trace in a plane cuts a slot: every return current above it detours round the slot and the slot radiates. |
| R2 | **Return via at every reference change** (mandatory for RF, the DC/DC switch node and the crystal; best effort for I2C, SWD (jig only), static GPIO and power, whose edges are slow enough that a 1-2 mm return detour costs nothing measurable). A signal that moves between the L1/L3 group and the L4/L6 group gets a GND via joining L2 and L5 (through `''` or buried `2-5`) within **0.6 mm** of its signal via. Moves inside a group (L1↔L3, L4↔L6) need none. | The return current has to jump planes with the signal; without a nearby GND via it spreads through plane capacitance and couples to everything. |
| R3 | **L3 runs north–south, L4 east–west.** Different nets on L3 and L4 may cross but not run on top of each other for more than **0.5 mm** (overlap with less than 0.10 mm lateral offset). | L3/L4 are 0.19 mm apart with no plane between: broadside coupling is the worst crosstalk in this stack. |
| R4 | **Net classes and widths** (minimum, wider where it fits): RF_50R 0.124 CPWG on L1 only; SW_NODE (DCC, DCC_L) 0.20, shortest path, L1 only; POWER (VSYS, VDD_1V8, VIN, BQ_VDD, VINLS, CELL_P/N, VDD_IMU, PMID; U12_VDD is a uA supply behind 330 ohm and routes as a signal) 0.20 trunk / 0.15 last 0.5 mm into a 0201 pad; DECOUPLE (DEC1/3/4, PPG_VLDO, PPG_VREF) 0.15 (0.12 inside the 0.4 mm WLCSP escape); CLOCK (XC1, XC2) 0.10; ANALOG (TH1/TH4/TH5_SENSE, VSYS_SENSE, TS) 0.10; DIGITAL (I2C, GPIO, SWD, INT, NRESET) 0.10. | Width sets inductance and IR drop for power; clock and analog stay thin to keep their area and capacitance small. |
| R5 | **Spacing by class** (measured outside the U1 ball field, where 0.4 mm pitch makes 0.30 impossible; decoupled supplies near the switch node are a warning, analog and digital victims a failure): default 0.10 (fab 0.09); SW_NODE and CLOCK keep **0.30 mm (2H)** from any other net on their own layer and no other net on the adjacent signal layer over them; ANALOG keeps 0.30 from SW_NODE / CLOCK / DIGITAL edges. RF keeps its 0.127 CPWG gap to GND and >= 0.5 to any other net. | Crosstalk falls with spacing/height; H = 0.10–0.15 mm here. |
| R6 | **Switch node short and shielded.** WLCSP ball B6 has no L1 escape, so DCC drops through its in-pad via to L3 (Nordic's CIAA DC/DC reference does the same on its first inner layer: blind via, ~1.3 mm, back up at the inductor) and comes straight back up in the 10 uH pad. Keep it under ~2 mm, 0.20 wide, L2 solid above it and nothing on L4 over it. | The DC/DC edge (~ns) is the strongest emitter on the board; buried between GND (L2) and an empty L4 it is shielded on both sides. |
| R7 | **Decoupling**: cap within 0.5 mm of its pin, GND end via-in-pad (1-2 or 5-6 microvia) straight into the plane, power trace from pin to cap before it goes anywhere else. | Mounting inductance, not capacitance, sets decoupling above ~50 MHz. |
| R8 | **Stitching**: GND vias on the island perimeter every <= 1.0 mm (λ/20 at 2.44 GHz in this stack is ~3.4 mm; tighter because the edge sits next to skin and the antenna), RF via fence both sides of the CPWG at <= 0.6 mm, and outer-layer GND fills stitched at <= 1.5 mm with no unstitched islands. | Stops edge radiation and the parallel-plate resonance between L2 and L5. |
| R9 | **Geometry**: 0° / 45° / 90° only (arcs on RF), no acute angles, no 45° run longer than 1.0 mm unless it is an escape, tracks end on pad / via centres, >= 0.30 mm from the board edge for signals (GND pour to 0.20). | Professional, manufacturable, and edge-coupled radiation stays low. |
| R10 | **External lines** (flex pads JR/JP: SWD, I2C, INT, CHG_PG, VSYS) leave the landing on an inner layer (L3/L4) or on L6 over L5 with a return via at the landing GND pads. | The flex is a 2-layer antenna; its return must stay tight to the signal into the island. |

## Both islands

- Solid L2 and L5 ground (R1); stitch GND around the perimeter at <= 1.0 mm (R8).
- 0201 passives, consistent orientation, a 0.5 mm placement grid, silkscreen designators where they fit (otherwise the assembly drawing).
- Test points (0.5 mm, bottom side where possible): VSYS, VDD_1V8, VDD_IMU, CELL+, SWDIO, SWDCLK, TP1 nRESET.
- Fillet the outline corners where the straight inner edge meets the arc (0.3 mm).
