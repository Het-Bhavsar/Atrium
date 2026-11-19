# Flex jumper (FPC1)

Outline: `hardware/design-data/outline/fpc_outline_edge_cuts.dxf`. Pads and contact openings: `atrium_outlines.dxf`
(layers `FPC_PADS_*`, `CONTACTS_FLEX_BOT`). All coordinates are in `hardware/design-data/outline/`.

## Build spec (JLC FPC)

| Item | Value |
|---|---|
| Layers | 2, no impedance control (no RF on the flex) |
| Thickness | 0.11 mm finished, ±0.05 (JLC 2-layer: 25 µm PI core, Cu 12 µm (1/3 oz) both sides, yellow coverlay PI 12.5 µm + AD 15 µm each side; JLC has no 12.5 µm core, checked 2026-11-17, O-9 closed). With the 0.03 mm PSA to the stiffener the slot is 0.14 (geometry `FPC_T`); the 0.02 over the old 0.12 came out of the cell's swell air (owner 2026-11-17). |
| Finish | ENIG on both sides |
| Stiffeners | One: 0.10 mm stainless steel, supplied and bonded by JLC on the flex top (order option Stainless steel 0.10; remark: SUS430, magnetic, preferred). The flex body is x 4.75-13.30, y 1.45-9.55 (8.55 x 8.10; widened 2026-11-17, the added area is copper-free) with four NPTH D0.60 (H1-H4) at (5.70 / 12.35, 2.025 / 8.975). JLC's CAM (production file Y65, 2026-11-19) makes the steel 0.10 smaller per side, x 4.85-13.20, y 1.55-9.45 (8.35 x 7.90, R0.2 corners), with the four pin holes opened into 0.60 U-slots to the long edges (a hole would leave a 0.175 web). Base pins D0.45 pass through flex holes and steel slots (+-0.075) and are glued. Spreads the pogo load and carries it into the base. No stiffener on the landing ends (the islands back the joints) or in the bend zones; the bends start at the flex body ends. Drawing: mechanical/pod/drawings/flex_stiffener_drawing.pdf (rev D). |
| Bends | Two S-bends, one per end, formed after soldering, static. Rise 0.78 mm (flex under the stiffener, z 4.78 -> island bottom z 4.00; 0.80 before the 0.11 JLC flex, 2026-11-17) over 0.85 mm run at the power end (r ~ 0.43) and 0.76 mm at the radio end (r ~ 0.38) (two tangent arcs: r = (run^2 + rise^2) / (4 rise)). The run is landing-pad edge to stiffener edge. The radio run can't grow: that bend sits 0.21 mm under the cell overhang and may rise only 0.10 there before it meets the cell (37 um margin now; 0.81 mm would leave ~5 um). **Bend zones are single copper layer**: x 3.90-4.75 (power) and 13.30-14.06 (radio), all 10 lines on one layer, no plane, no vias, no pads, no coverlay or stiffener overlap; traces cross at 90 deg. On JLC's 25 µm core a single-copper bend zone is ~0.098 mm, so r is 3.9-4.3x t, under JLC's 6x rule of thumb; copper strain is 3.3-3.4 %. Exception: GND from JP6 crosses the power bend on the bottom layer (the 2 x 5 grid has 9 top exits for 10 nets), 8.0 % strain against ~11 % for a one-time install bend. Keep both coverlays. Form once over a 0.4 mm rod and test continuity (O-25). If JLC needs more, the only lever is length: +0.1 mm of L per end for each 0.1 mm of run. The power bend sits 0.24 mm under the cell (26 um margin). |

## Pads: 10 per end, 0.45 x 0.50 mm, plated, each with a via-in-pad (solder wicks through; reheat from the top)

Power end, trimmed 2026-11-17 so the flex clears the power island's bottom parts at its landing (C53 0.11, C55 0.13,
U10 0.15, D10 0.15, R51 0.145 mm; were 0.01-0.06): tongue end at x 2.80 and lower edge at y 3.445 (assembled). The
outer column (GND, I2C_SDA, I2C_SCL, GND, SWDCLK) is 0.35 wide centred at x 3.175, inside the 0.45 island pads; the
bottom row (SWDIO, GND) is 0.425 tall (y 3.645-4.07). Those six pads carry 0.15 / 0.35 mm vias, the other 14 keep
0.25 / 0.45. Copper stays 0.20 from the outline. The radio tongue is y 3.05-7.95.

| End | Pads |
|---|---|
| Power (island bottom, two rows at the inner edge, x 3.125 / 3.675) | SWDIO, VSYS, VIN, CHG_PG, IMU_INT1 / GND, I2C_SDA, I2C_SCL, GND, SWDCLK |
| Radio (island bottom, one tongue at the inner edge, columns x 14.285 / 14.885, y 3.5–7.5) | GND, VSYS, GND, CHG_PG, IMU_INT1 / GND, I2C_SDA, I2C_SCL, SWDIO, SWDCLK |

## Contact pads (flex bottom, coverlay openings, exposed through base holes)

| Pad | Pod coords (mm) | Net |
|---|---|---|
| d1.0 | (10.45, 5.50) | VIN |
| d1.0 | (8.25, 5.50), (12.65, 5.50) | GND (pitch 2.2 from VIN; GND_B pad edge 13.15, 0.15 inside the stiffener) |
| d1.0 | (6.70, 3.30) | SWDIO |
| d1.0 | (6.70, 7.70) | SWDCLK |

## Traces

| Nets | Width |
|---|---|
| VSYS, VIN, GND | 0.30 mm minimum |
| Signals | 0.10 mm |

Put GND on the second layer under the I2C pair where width allows. VIN runs from the centre pad to the power end. SWD runs from each contact to BOTH ends: to the radio end (nRF pins) and to the power end, where D12/D13 clamp it to GND at the landing (2026-11-08: SWD ESD moved off the radio island, 47 ohm series resistors dropped).

## Assembly

1. Tin the island landing pads, add flux.
2. Align the flex on its fiducials and hold it with Kapton.
3. Iron through the via-in-pads (fine chisel, under a microscope), or hot-bar in production. **No hot air**: U1 and U3 are millimetres away.
4. Continuity-check all 10 lines at each end.
5. Form the S-bends on a jig.
6. Bond the cell with thin double-sided tape (the coverlay insulates it) and solder the tabs.
