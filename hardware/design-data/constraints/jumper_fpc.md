# Flex jumper (FPC1)

Outline: `hardware/design-data/outline/fpc_outline_edge_cuts.dxf`. Pads and contact openings: `atrium_outlines.dxf`
(layers `FPC_PADS_*`, `CONTACTS_FLEX_BOT`). All coordinates are in `hardware/design-data/outline/`.

## Build spec (JLC FPC)

| Item | Value |
|---|---|
| Layers | 2, no impedance control (no RF on the flex) |
| Thickness | target <= 0.12 mm total over the body (adhesiveless PI 12.5 µm, Cu 12 µm (1/3 oz), coverlay 12.5 µm) [VERIFY JLC options, O-9] |
| Finish | ENIG on both sides |
| Stiffeners | 0.1 mm PI under both landing ends, on the side away from the pads. 0.1 mm SUS430 under the contact zone (x 4.75-13.30, y 2.65-8.35) between flex and cell: spreads pogo load off the pouch and is the Plinth magnet target (mechanical, geometry `STIFFENER`). No stiffener in the bend zones. |
| Bends | Two S-bends, one per end, formed after soldering, static. Rise 0.80 mm (flex under the stiffener, z 4.80 -> island bottom z 4.00) over 0.85 mm run at the power end (r ~ 0.43) and 0.76 mm at the radio end (r ~ 0.38) (two tangent arcs: r = (run^2 + rise^2) / (4 rise)). The run is landing-pad edge to stiffener edge. The radio run can't grow: that bend sits 0.21 mm under the cell overhang and may rise only 0.10 there before it meets the cell (37 um margin now; 0.81 mm would leave ~5 um). **Bend zones are single copper layer**: x 3.90-4.75 (power) and 13.30-14.06 (radio), all 10 lines on one layer, no plane, no vias, no pads, no coverlay or stiffener overlap; traces cross at 90 deg. At ~0.07 mm there that is ~5.8x t (IPC-2223 guidance for one copper layer, bend-to-install, is ~6x t; a 0.12 mm two-layer build would be 3.4x t, too tight). [VERIFY JLC minimum static bend radius for 1-layer adhesiveless PI.] If JLC needs more, the only lever is length: +0.1 mm of L per end for each 0.1 mm of run. The power bend sits 0.24 mm under the cell (26 um margin). |

## Pads: 10 per end, 0.45 x 0.50 mm, plated, each with a 0.25 mm via-in-pad (solder wicks through; reheat from the top)

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
