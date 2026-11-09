# Open items

The engineering questions still open on the EVT pod and charging case, with where each one is closed. Status as of 2026-11-09; close each item with its source (datasheet section or measurement).

Build-specific items (B-1 to B-10) are in [build-assembly-test.md](build-assembly-test.md). `[VERIFY]` in any doc marks a value that is not yet confirmed.

## Datasheet checks before schematic freeze

| # | Item | Where to check |
|---|---|---|
| O-1 | nRF52832 CIAA DC/DC reference values and positions (C3 1.0 pF shunt, C6/C7 100 pF, DEC2 left open?) | nRF52832 PS, Reference circuitry, Fig 5 / Table 5 |
| O-2 | BQ21061 VINLS/LSLDO termination when the LDO is unused | TI BQ21061 EVM schematic |
| O-3 | BQ21061 default ICHG out of reset, ICHG register address, ship-mode command | TI datasheet register map |
| O-4 | AP6683 thresholds (VCU, VDL 2.8 V, discharge OCP), RDS(on), VDD R/C values | ALLPOWER datasheet (LCSC C2849565) |
| O-5 | MAXM86161 PART_ID value (0x36?), INTB polarity | ADI datasheet register map |
| O-6 | TPS7A02 minimum output capacitance (1 µF 0201 fitted) | TI datasheet |
| O-7 | KH-1608-H08 feed and ground pad layout inside our clearance | Kinghelm spec (layouts 1–3, 5 × 3 to 5.5 × 4 mm) |
| O-8 | Closed 2026-11-08: JLC06081H-2313 6-layer 0.82 mm, CPWG 0.124 mm / 0.127 mm gap (JLC impedance calculator) | – |
| O-9 | JLC flex: adhesiveless option, 1/3 oz Cu, PI 12.5 µm, ENIG on both sides, PI stiffener | JLC FPC capabilities |
| O-10 | Zephyr: `st,lsm6dso` binding properties for the LSM6DSOX; MLC via ST's tools | The Zephyr version you build with |

## Added by the 2026-11-07 review

| # | Item | Where to check |
|---|---|---|
| O-22 | AP6683 datasheet: pinout, thresholds, and whether R51/C55 belong (the LCSC listing says R and C are inside; its PDF link is empty). Fallback: LPB1010 | ALLPOWER / LCSC |
| O-23 | KH-1608-H08 datasheet: dimensions (LCSC says 1.7 long), feed/GND pads, clearance | Kinghelm |
| O-24 | C53 4.7 µF 0402, LCSC number C23733 | LCSC |
| O-25 | JLC FPC minimum static bend radius for 1-layer adhesiveless PI (bend zones r about 0.41–0.43 mm, about 5.8 × t) | JLC FPC capabilities |
| O-26 | Cell tab positions on the 400909 drawing (placement assumes pads at y 3.2 / 7.8 at the power inner edge) | Sharvi drawing |
| O-27 | ~~Zirconia coupon on the VNA~~: the production shell is now PC with NCVM, see O-36 (zirconia only for a premium edition) | VNA |
| O-28 | Plinth magnet pull on the 0.1 mm SUS430 stiffener against two pogos at ≤ 0.3 N each | Bench |
| O-29 | KH-1608-H08 land pattern: the library footprint has 1.15 mm pads on a 0.93 mm body; the antenna sits with its pads 0.2 mm inside the tip edge and 0.07 mm inside the zone | Kinghelm drawing |
| O-30 | The radio island is at capacity: one of eight placement orders fits. Check fan-out and via room on the first routing pass before adding anything | Layout |
| O-31 | MAXM86161 land pattern 90-100106 / outline 21-100309: the library footprint has its pad columns at ±0.95 mm (0.66 × 0.40 pads), the placement study used ±1.2 mm, and the datasheet gives neither. Confirm the pads and the body at x 15.66–18.56 | ADI package drawing |
| O-32 | 6-layer HDI 1+4+1 (JLC06081H-2313, 0.82 mm): buried-via and lamination fees are "manual quote" on the order page. Confirm the stacked L1–L5 skip via (laser on buried) and filled via-in-pad on the WLCSP / DSBGA with JLC engineering before ordering | JLC order page / engineering |
| O-33 | WLCSP-50 ball land with a 0.25 mm microvia capture pad (the library ball pad is 0.17 mm): check against Nordic's CIAA land recommendation | nRF52832 PS, mechanical / land pattern |

## EVT measurements

| # | Item | Pass criteria |
|---|---|---|
| O-11 | 10 × 400909 cells: dimensions and capacity at 0.2 C | All ≤ 9.2 × 9.2 × 4.05 mm; capacity known (the power budget assumes 30 mAh × 80 %) |
| O-12 | Contact pads in artificial sweat (ISO 3160-2), 72 h worn | No visible corrosion, < 1 µA leakage |
| O-13 | Antenna: S11 with shell, cell and head (or phantom); tune the pi-match | ≤ −10 dB across 2402–2480 MHz |
| O-14 | OTA RSSI at 1 / 3 / 5 m, phone in hand / front pocket / back pocket | Compare with the link budget in `hardware/simulation/` |
| O-15 | Average current with a PPK2 in each mode | Compare with the power budget in `hardware/simulation/` (189 µA typical) |
| O-16 | VLED headroom: lowest VSYS at which the green PPG SNR holds | Sets the firmware optical cut-off (placeholder 3.3 V) |
| O-17 | NTC accuracy against a reference thermometer, 20–45 °C | ±0.3 °C after the R–T table replaces the beta LUT |
| O-18 | HRV against a chest-strap ECG: 25 sps with interpolation vs 100 sps windows | RMSSD bias < 3 ms at rest |
| O-19 | Skin pad wear: 5–10 people, desk day, run, shower, side-sleep; days per pad (1–2 estimated), edge lift, removal comfort | No lift-off within a day; no irritation |
| O-20 | ~~Skin cradle retention~~: superseded by the skin pad (2026-11-07) | – |
| O-21 | Swell: 300 cycles, measure cell growth | ≤ 0.2 mm (the swell gap) |
| O-34 | Skin pad tape: double-coated medical, opaque skin-tone, about 0.15 mm total (the bed depth follows the tape) | 3M / Solventum data sheets; converter |
| O-35 | Pad converter: die-cut registration ≤ 0.10 mm so the pad lands in the bed off the card | Converter sample |
| O-36 | NCVM silver shell: antenna return loss and efficiency, bare vs coated shell; re-tune the match if it moves | VNA, with O-13 |
| O-37 | Shell rim hardcoat on skin: ISO 10993-5/-10, or mask the rim during NCVM | Coating supplier |
| O-38 | Board clamp vs island thickness 0.82 ± 0.10 mm: the ribs and posts are rigid, so a 0.92 mm board meets 0.10 mm of interference and a 0.72 mm board rattles. Options: a compliant pad on each rib tip (for example 0.2 mm silicone foam at 25–75 % compression) or crush bumps. 2026-11-08: solder-mask openings were added on both islands, both sides, at the clamp bands, which takes the nominal crush from 0.024 mm toward 0 [VERIFY on a measured board; the board STEP still exports a single 0.844 mm slab] | First PCB lot, calipers |
| O-39 | Closed 2026-11-08: the flex fab outline is the flat pattern, with the lands moved 0.47 mm (power) and 0.51 mm (radio) outboard to give each S-bend its bend allowance; the mechanical fit check passes | – |
| O-40 | Charging case: measure the KP-502030 (thickness, lead exit and gauge, PCM; the CAD reserves 5.0 mm and runs the leads to holes at (−15.5, ±2.0)); pick the SMD pogo (free 3.25, 1.0 stroke, ≤ 0.3 N at 0.68); the lid foam (1.0 thick, 1.5–2.7 N at 25 %); HAL248 operate point vs about 18 mT; SKSCLAE010 stem centre height and width (the CAD assumes 0.6 / 1.2); USB-C C2765186 STEP vs the 17.99 mouth; and whether a 0.2 mm recessed 2.8 × 1.4 mm cap is pressable with a fingertip (print it) | Sharvi sample, pogo and foam data sheets |
| O-41 | Optical module code. The BOMs order MAXM86161EFD+T (LCSC C3178309). DigiKey lists it as obsolete (2026-11-09); LCSC had 84 in stock, enough for one islands panel run (36 pods), so buy the EVT parts now. The active code is MAXM86161AEFD+T (DigiKey 175-MAXM86161AEFD+TCT-ND, 3,037 in stock), not yet in the JLC catalogue. Analog Devices does not state that it is a drop-in [VERIFY: same OLGA-14 land pattern, pinout, PART_ID and register map]. After that, change U3 in the boards and BOMs and supply it to JLC as a consigned part | Analog Devices support or PCN; first A-code part on the bench |
| O-42 | Closed 2026-11-09: version text on the bottom silkscreen, 1.0 mm characters: `ATRIUM EVT v0.1.0 2026-11` on the charging-case board and `EVT v0.1` on the flex (inside every pod). The radio and power islands have no clear strip for text of a legible size (the largest fits 0.55 mm characters), so the pod is marked through its flex. DRC unchanged apart from the known accepted items | – |
