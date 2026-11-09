# Build, assembly and test

How the first pods and charging cases are fabricated, assembled, flashed and debugged, step by step, with the checks and simulations behind each step. Status 2026-11-09; every `[VERIFY]` is an open question to close before the order or on the first boards.

## Inputs

- **Routed boards:** radio and power islands, flex jumper, charging case. DRC: radio 0, power 0, flex only the known JP6–JP9 link that crosses boards, case only two items inside the USB-C footprint (accepted).
- **Mechanical design**, fit-checked against the real board STEPs ([mechanical.md](mechanical.md)).
- **Process checks behind this plan** (reports in `hardware/simulation/`):
  - stencil area ratio per pad, computed from the paste Gerbers per IPC-7525;
  - cell-lead soldering heat, and bottom-side part retention in a second reflow;
  - worn and charging temperature.

**Industry practice this plan follows:**
- Stick-on wearables commonly over-mould or pot the board, hold the cell on PSA foam and never glue the pouch.
- Medical adhesive tapes quote 3 days (silicone / acrylic on a film, for example 3M 2477P) to 14 days (for example 3M 4075 / 4076) of wear, with ISO 10993-5/-10 data.
- Cell makers spot-weld tabs at the factory; you solder to the welded tab, never to the cell, at about 350 °C for ≤ 3 s, and keep the cell under about 100 °C.
- Production flashing of nRF52 parts is done through pogo fixtures with a production J-Link and nrfjprog / nRF Util. A DK's on-board debugger is licensed for development only.
- Metallic finishes that pass RF: NCVM on PC. Plating and PVD are conductive.

## Build order at a glance

| # | Pod | Charging case |
|---|---|---|
| 1 | Order the islands (6-layer HDI) and the flex (2-layer PI) from JLC; islands with JLC PCBA, both sides | Order the 2-layer 0.8 mm board with JLC PCBA, one side |
| 2 | Incoming check: X-ray report, visual, island current at 5 V with no cell | Incoming check, visual |
| 3 | Flash and bring up each island pair on the bench harness (before the flex) | Flash the CH32V003 over SWIO, then bring up on USB |
| 4 | Join the flex to both islands (hot-bar or fine-tip hand solder) | Fit the cell leads through the plated holes, solder |
| 5 | Fit and solder the cell (clip on the tab) | Mechanical assembly: tub, PCB, deck, pogos, lid |
| 6 | Full test in the programming jig | End-of-line test with a golden pod |
| 7 | Mechanical assembly: shell, gasket, gel, base, perimeter bond | Finish check, packaging |
| 8 | Final test: seal, RF, PPG, charge in the case | |

## 1. Fabrication release

| Board | Fab spec | Before ordering |
|---|---|---|
| Radio + power islands | JLC06081H-2313, 6-layer HDI 1+4+1, 0.82 mm, ENIG, impedance control (CPWG 0.124 / 0.127), Tg170 | Buried-via and lamination fees are "manual quote"; confirm the stacked skip via and filled via-in-pad on the WLCSP / DSBGA with JLC engineering (O-32). WLCSP microvia capture pad vs Nordic's land (O-33). Mask openings on both sides of the six clamp bands. |
| Flex jumper | 2-layer polyimide, 0.0865 mm, ENIG both sides, flat pattern (bend allowances 0.47 / 0.51 mm), 0.1 mm SUS430 stiffener | Single-copper-layer bend zones; JLC minimum static bend radius vs r 0.38–0.43 mm (O-25); stiffener material options (O-9). No SMT parts on the flex. |
| Charging case | 2-layer FR-4 0.8 mm (35 µm / 0.71 core / 35 µm), 1 oz, ENIG or lead-free HASL, green or black mask | JLC 2-layer surcharge for 0.2 mm via holes [VERIFY]; two plated 0.8 mm cell-lead holes at (−15.5, ±2.0). |

Production files are in each board's `production/` folder: `hardware/pod/radio-island/production/` (`Atrium_Radio_Island_gerber.zip`, `Atrium_Radio_Island_BOM.csv`, `Atrium_Radio_Island_CPL.csv`), `hardware/pod/power-island/production/`, `hardware/pod/flex-jumper/production/`, `hardware/pod/islands-panel/` and `hardware/case/charging-case/production/`.

### 1a. Islands panel (sampling and SMT in one order)

Both 6-layer islands go on one panel so they share one PCB order, one stencil pair and one PCBA setup. The panel is a PCB design of its own in `hardware/pod/islands-panel/`, built from the radio and power island boards. It was checked part by part against both source boards and, as Gerbers, against an independently merged Gerber set: identical except the copper keep-outs at the tab joints. Production files: `Pod_Islands_Panel_gerber.zip` (with the adjusted paste apertures), the panel BOM and CPL, and a panel preview image.

- **45.0 × 47.4 mm, 2 × 3 sets = 6 pods.** 5 mm frame, 3 mm bar between columns, 3 asymmetric fiducials per side, 4 × 1.152 mm tooling holes.
- **Tabs:** every pod is held at both ends. The radio island hangs from the bar (or the right frame) by two tip tabs at −29° / +28°, inside the copper-free antenna zone. The power island holds to the frame (or the bar) on its left by one tab at 133°, 0.45 mm from U12 and 15° clear of the clamp band. The two islands are joined by two corner tabs. There is only one outer tab on the power island: on the lower arc U10's DSBGA balls and the 0201s sit 0.22–0.32 mm from the edge, too close to cut a tab. Mouse bites 0.50 mm at 0.65 mm pitch, centres 0.30 mm outside the edge (84 per panel). Nearest pad to a tab end: 0.40 mm (radio inner corner).
- **Stiffness check** (islands rigid, tabs as beams, the 0.15 mm web between two bites as the weak link, 2 N nozzle force anywhere on an island): worst deflection 0.93 mm with the radio tips alone, 64 µm with the 133° power tab, 25 µm with a second power tab. The second tab isn't used: its only spot (227°) is 0.22 mm from R50 and 0.31 mm from C53 (0402 MLCC, crack-prone). Ask for board support under the frame and bars on both passes; deflection scales with the nozzle force.
- **Checks on the merged Gerbers:** no copper or paste outside the islands, the panel is one piece of material, and all 312 placements (174 top, 138 bottom) land on their own island.
- **Depanel by cutting the tabs, never snapping.** WLCSP / DSBGA balls sit within a few mm of the tabs on a 0.82 mm board. Sand the nubs flush.
- **CPL** matches the PCB tool's own pick-and-place export of the panel (position within 0.002 mm, same side and rotation for all 312 parts). An earlier version had D10 / D12 / D13, U2, U12, U13 and L1 wrong by 45–180°; fixed. Still check the fine-pitch parts in JLC's preview (B-9).

- **Gerbers:** export straight after a pour refill, then render and check them (for the case: both pours solid, the outline one contour, the drill file with 0.2 / 0.6 / 0.8 mm tools).
- **Electrical test:** keep JLC's flying-probe test on.
- **Mask colour on the islands:** black near the optical module cuts stray light into the photodiode [VERIFY against the MAXM86161 layout notes].

## 2. Stencil and SMT

Area ratio = aperture area / (perimeter × foil thickness). It needs to be ≥ 0.66 for a clean release from a laser-cut foil; 0.5–0.66 needs an electropolished or nano-coated foil; below 0.5 the paste won't print.

| Board | Side | Apertures | Worst aperture | AR on 0.08 mm | 0.10 mm | 0.12 mm |
|---|---|---|---|---|---|---|
| Radio | Top | 101 | U1 WLCSP ball 0.17 × 0.17 | 0.57 | 0.46 | 0.38 |
| Radio | Bottom | 49 | U2 DSBGA ball 0.17 × 0.17 | 0.57 | 0.46 | 0.38 |
| Power | Top | 28 | 0201 diode 0.30 × 0.20 | 0.78 | 0.62 | 0.52 |
| Power | Bottom | 55 | U13 DSBGA ball 0.157 (U12's exposed pad prints: see below) | 0.57 | 0.46 | 0.38 |
| Flex | – | – | No parts: **no stencil** | – | – | – |
| Case | Top | 174 | U1 QFN lead 0.60 × 0.20 | 0.96 | 0.77 | 0.64 |

The table gives the apertures as designed, before the changes below.

**Decisions**
- **Islands: 0.08 mm electropolished foil** (nano-coated if JLC offers it [VERIFY]), with the fine-pitch apertures opened up in the paste Gerbers (done 2026-11-09):
  - U1 nRF52832 (0.4 mm pitch, 0.17 mm pads): 0.21 mm, AR 0.70 on the 0.08 foil.
  - U10 BQ21061 (0.4 mm, 0.20 mm pads): 0.22 mm, AR 0.69.
  - U2 / U13 TPS7A02 (0.35 mm, 0.157 mm pads): 0.20 mm, AR 0.67.
  - **U12 AP6683 exposed pad:** the full 0.48 mm opening (100 %) can float a 1 × 1 mm DFN, so it is trimmed to 0.38 mm (about 63 %) [VERIFY against the AP6683 land pattern, O-22].
  - **How:** the PCB tool keeps one paste expansion per board and drops per-pad paste edits on reload, so the change is made in the exported paste layers. Only the listed pads are scaled, the pad count is checked against the design, and the area ratios are re-checked per IPC-7525 before the Gerbers are zipped. The Gerbers in each `production/` folder already include the change.
- **Case: 0.10 mm foil**, no changes (worst AR 0.77).
- **Flex:** don't order a stencil. Its bottom paste layer covers the five ENIG skin contacts, which must stay solder-free (corrosion, O-12). Its top paste layer is the jumper landing, which is soldered after PCBA (section 3).
- **Double-sided islands:** both sides go through reflow, and the second-pass parts hang upside down. The retention simulation puts every bottom-side part far below the 30 g/in² limit (MAXM86161 6.7, BQ21061 3.8, AP6683 2.2), so no glue dots are needed.
- **Reflow:** SAC305, peak 240–250 °C, 45–75 s above liquidus. Bake moisture-sensitive parts before reflow if their bags are open (MAXM86161 and the WLCSP MSL [VERIFY]).
- **Inspection:** ask for X-ray on U1, U2, U10, U13 and the MAXM86161 (OLGA-14, hidden pads). AOI on the rest.
- **JLC capability to confirm before the order [VERIFY]:**
  - 0.4 mm-pitch WLCSP and 0.35 mm DSBGA;
  - 0201 parts on both sides of a 0.82 mm HDI board;
  - an OLGA optical module placed with its lens tape on.
- **Flux and cleaning:** no-clean paste, but keep flux off the MAXM86161 window. Leave its protective film on until final assembly [VERIFY the module ships with one]; clean with IPA if needed.

## 3. Pod: flex to islands

The flex has 10 landing pads at each end (JP1–10 power, JR1–10 radio) that solder to the matching pads on the island bottoms.
- **At PCBA:** the island paste layers include the landing pads, so the islands come back with solder on them.
- **Join (EVT, by hand):**
  1. Put both islands and the flat flex in a fixture: a 3D-printed nest that holds the island edges, and two dowel-located clamp plates.
  2. Add tacky flux and align each end under a microscope.
  3. Reflow each landing: hot-bar if available, otherwise hot air at about 300 °C with a 3 mm nozzle, or a fine chisel tip drawn along the row.
- **Check:** continuity on every jumper net, and a pull test on scrap: peel ≥ 3 N per landing [EST]. No solder may reach the bend zones; they start 0.47 / 0.51 mm past the stiffener ends.
- **Bend:** form the two S-bends over a 0.4 mm radius pin only after the joints are done. Never crease a bend.

## 4. Pod: bring-up before the cell (bench harness)

1. Before power, measure resistance to GND from VIN, VSYS, VDD_1V8 and VDD_IMU. Each should read > 1 kΩ.
2. **Jig:** the four probes on VIN, GND, SWDIO and SWDCLK ([flashing-and-test.md](flashing-and-test.md)).
   - **Supply:** VIN = 5 V with a 50 mA limit.
   - **Probe:** J-Link with VTref fed from the jig's 1.8 V. SWD is 1.8 V; never use a fixed 3.3 V probe.
3. **Rails:** VIN current under 20 mA with no cell. VSYS 4.5 V and VDD_1V8 1.80 V at the test points (0.5 mm pads: VSYS, VDD_1V8, VDD_IMU, CELL+, SWDIO, SWDCLK, nRESET).
4. **Flash:** `nrfjprog --recover`, then MCUboot plus the bring-up build (`west flash --runner jlink`). At 4 MHz SWD the image (MCUboot + app, about 250 KB) takes about 5–10 s [measure].
5. **RTT check:**
   - all three I2C parts answer: IMU 0x6A, MAXM86161 0x62 and PART_ID, BQ21061 0x6B while VIN is present;
   - TH1 and TH4 read near room temperature.
6. **RF:** a conducted check through a pigtail at the pi-match pads, then RSSI from a phone ("Atrium" advertising). Tune the 3.3 nH / 1.0 pF match on the VNA (O-13).

**Debug kit:** J-Link (production licence) with RTT, a Nordic PPK2 for current profiles (compare with the power budget in `hardware/simulation/`, 189 µA typical), nRF Connect on a phone, a microscope and a hot-air station.

| Symptom | Likely cause | Check |
|---|---|---|
| No SWD connection | No VIN, VTref not 1.8 V, or APPROTECT set | Jig 5 V and 1.8 V present; `nrfjprog --recover` |
| VIN current > 50 mA | Short at a WLCSP / DSBGA ball or a 0201 | Thermal camera or IPA-evaporation spot; X-ray |
| I2C device missing | Ball joint open, or its rail missing | VDD_IMU / VSYS at the test pads; X-ray U10 / U13 |
| BLE range poor | Match off, or a coated shell detuning | VNA S11 bare vs coated shell (O-13, O-36) |
| PPG flat or saturated | Window film left on, gasket gap, LED rail low | Remove film; check VLED headroom (O-16) |

## 5. Pod: fitting the cell (the risky joint)

A 1-D fin model of the cell lead, from the joint to the pouch seal, gives:

| Lead | Joint to seal | Bare: peak at 1 s / 3 s | Max dwell for ≤ 80 °C | With a copper clip on the tab |
|---|---|---|---|---|
| Ni tab 3 × 0.08 mm (pod) | 2.5 mm | 116 / 188 °C | 0.4 s (3 s if heat spreads into the stack) | > 8 s |
| Cu wire 28 AWG (case) | 5 mm | 48 / 77 °C | 3.2 s | > 8 s |

The model is pessimistic: the joint is held at 250 °C and the seal's heat capacity is small. Even so, the pod cell can't take a normal 2–3 s hand joint. Procedure:
1. Order cells with the tabs spot-welded by the maker, ideally with a nickel extension tab, so the joint sits 5 mm or more from the seal. Confirm tab width, length and position on the Sharvi drawing (O-26).
2. Fit the cell last, after every reflow, cleaning and test step. The bring-up in section 4 runs without it.
3. Pre-tin the island pads (CELL+ / CELL−, 1.0 × 1.2 mm) and the tab ends, with flux.
4. Clip flat copper tweezers or a crocodile clip on the tab between the joint and the seal.
5. One touch, 1 s or less, fine tip at 330 °C. Let it cool before any second touch. Never touch the seal.
6. Kapton over each joint. Fold the tabs as the bay drawing shows. Nothing rigid may press on the pouch.
7. Check: cell voltage at the CELL+ test pad, and charge current about 15 mA once the firmware sets ICHG. CHG_PG should go low on VIN.

**Swell:** the bay leaves 0.2 mm. Measure 10 cells before freezing the shell (O-11), and cycle 300 times (O-21). Hold the cell with the bay, not glue. If a tape is needed, use thin PET double-coated PSA and only where the z-stack has room [VERIFY against the stack].

## 6. Pod: mechanical assembly

From [mechanical.md](mechanical.md):
1. Shell upside down in a soft nest.
2. Lower the islands, flex and cell in as one assembly. The islands sit on the shell ribs at their clamp bands (radio ±60° / ±86°, power 114° / 246°).
3. Window gasket (black LSR, or cut black silicone for EVT) round the MAXM86161, flush with the bed floor.
4. Thermal gel on the TH1 boss: fill the 0.19 mm gap (raise the boss 0.09 mm once the real TH1 height is confirmed).
5. Base on top: its seven posts clamp the islands. Check the contacts sit centred in their five holes.
6. **Perimeter bond (the sweat seal):** medical UV-cure adhesive [VERIFY grade] run all round the base joint, then UV cure. For EVT a thin bead of skin-safe epoxy works.
7. **Leak check:** submerge 10 min at 0.5 m with a dye wipe, or use a pressure-decay fixture [EST]. Then a final electrical test in the jig.

**Why not ultrasonic welding for EVT:** it is the fast production joint for medical wearables, but vibration at 21 × 11 mm risks the cell and the WLCSP joints. Keep adhesive for EVT and trial welding later on dummy builds.

## 7. Finish (pod shell)

Covered in full in [mechanical.md](mechanical.md#form-the-capsule):
- **Satin silver-grey that stays non-conductive** (the antenna is 0.3 mm behind the wall).
- **EVT:** SLA at 25 µm layers, sanded 400 → 800 → 1500, fine bead-blast, filler primer, mica or pearl silver (no aluminium flake), matte 2K clear. An NCVM shop can also coat SLA parts.
- **Production:** textured PC moulding, NCVM and a matte UV hardcoat.
- **Before choosing the coating:** measure S11 and efficiency with a bare shell and a coated shell (O-36).
- **Skin contact:** the base stays uncoated. Mask the rim, or get ISO 10993-5/-10 data for the hardcoat (O-37).

## 8. Skin adhesive

- **Pad:** the disposable 0.15 mm double-coated medical pad in the base bed, six to a card. Medical acrylic on the pod side; on the skin side, a high-tack acrylic or a silicone.
- **Candidates [VERIFY thickness ≈ 0.15 mm]:** 3M 1577 and 2477P. For longer wear, compare a 4075 / 4076-class tape (14-day rated).
- **Change interval:** every charge, when an edge lifts, or after 2 days. That sits inside the 3-day rating of the shorter-wear tapes and the pod's roughly 4-day battery.
- **EVT cutting:** kiss-cut the die-line (`mechanical/pod/DXF/skin_pad_dieline.dxf`) on a vinyl cutter or a CO₂ laser.
- **Production:** an ISO 13485 medical converter, with ISO 10993-5/-10 data.
- **Wear test (O-19):** 10 people, 8 h+ per day, desk day, run, shower, side-sleep. Record lift-off, residue and skin redness. Compare acrylic against silicone skin sides.
- **Peel matters more than load:** about 0.3 N at 10 g of acceleration on 130 mm² is only about 2 kPa. If edges lift, add the optional 1.0–1.5 mm clear PU skirt.

## 9. Charging case

1. **PCBA (JLC, one side, 0.10 mm foil):** CH32V003, TP4054, TPS61023, SY6280, translators, LDO, LEDs and button. D1 / D2 are B5819WS SOD-323 (C179554). In a pogo short the SY6280 limits at 0.34–0.57 A and the conducting diode dissipates 236–329 mW against a 200–250 mW rating, so firmware must cut VIN_EN within 50 ms of the 80 mA trip (B-8).
2. **Flash (before the cell):** WCH-LinkE on TP1 SWIO / TP3 GND, with TP2 at 3.3 V from the programmer. Firmware source is in `firmware/case/`.
   - Run `minichlink -w atrium_case.bin flash -b`.
   - Run `minichlink -D` once, so PD7 is a GPIO (blue LED), not NRST.
   - With a cell fitted, connect only SWIO and GND; don't back-feed VDD.
   - The MCU answers SWIO for 5 s after reset and whenever USB is in.
3. **Bring-up on USB (current-limited 5 V, 500 mA):**
   - VBUS_DET seen.
   - Charge current set by R5 4.7 kΩ: expect about 199 mA (179–219 mA). Above 150 mA the TP4054 data sheet uses R = 1000/I × (1.2 − 4/3 I), not 1000/I (charging tolerance analysis in `hardware/simulation/`).
   - The TP4054 runs hot at the start of constant current: about 0.34 W, 87 °C junction typical, 119 °C worst case, and thermal fold-back in about 5 % of builds. That is safe (the part folds back) but slower. R5 = 6.8 kΩ gives 147 mA, 26 % less heat and 3.5 h instead of 2.6 h (B-10).
   - Boost 5.2 V with BOOST_EN.
   - SY6280 limits at about 0.45 A into a short.
   - Pogo VIN switches with VIN_EN.
   - With a golden pod docked: the case reads presence through SWDIO and does a hard reset through CTRL-AP.
4. **Cell:** KP-502030 with wire leads (check: PCM present, lead gauge, exit position).
   - Leads up through the two plated 0.8 mm holes, past the cell end, soldered on top: red to BT1 (+), black to BT2 (−).
   - Pre-tin. 2–3 s per joint is within the simulated limit at 5 mm.
   - Small slack loop, a glue dot for strain relief, foam pad under the cell.
5. **Mechanical** (`mechanical/case/`):
   - **Tub:** cell on foam, PCB onto its ledges.
   - **Deck:** two magnets, four pogos with alignment collars, PCB clamp posts.
   - **Lid:** barrel hinge, snap detent, hold-down foam, lid magnet over the hall sensor.
   - Light pipe over D3 (0.41 mm air gap) and the recessed button cap.
6. **End-of-line test:**
   - LED patterns.
   - Button: short press, 5 s and 12 s holds.
   - Lid sensor.
   - Charge a flat pod.
   - Boost efficiency at 100 mA.
   - Standby current under 50 µA [EST; measure].

## 10. End-of-line tests

| Pod step | Check | Limit |
|---|---|---|
| 1 | VIN current, no cell | < 20 mA |
| 2 | Flash and verify | Pass |
| 3 | I2C IDs (IMU, MAXM86161 PART_ID, BQ21061 STAT0) | Pass |
| 4 | TH1, TH4 vs ambient | ±1.0 °C |
| 5 | Charge current at the 15 mA setting | 13–17 mA |
| 6 | RSSI at 1 m vs a golden pod | Within 4 dB |
| 7 | PPG: LED drive and photodiode counts on a grey reference target in the jig lid | Within ±20 % of golden [EST] |
| 8 | Ship mode, then wake on VIN | Pass |
| 9 | After sealing: leak check, then steps 1, 3 and 6 again | Pass |

There is no published factory procedure for PPG. A fixed grey target (or a simple optical simulator) in the jig lid is enough for EVT; a pulsing target can come later.

## 11. Tools and consumables for the first 10 pods and 5 cases

- **Hand tools:** fine-tip iron (0.2 mm conical, 1.2 mm chisel) with a 330 °C setting; hot-air station with a 3 mm nozzle; stereo microscope (10–40×); flat copper clip tweezers.
- **Consumables:** tacky flux (no-clean), 0.3 mm SAC305 wire, Kapton tape, IPA.
- **Programmers and meters:** J-Link (production licence); WCH-LinkE; Nordic PPK2; a 0.1 mm thermocouple for the dummy-cell soldering trial; USB-C power meter.
- **Jigs:** pod programming jig with P75-class probes (Ø1.02 mm) and a hold-down lid (`mechanical/programming-jig/`); flex-join nest; printed shell nest.
- **Adhesives:** medical UV adhesive and lamp; thermal gel; the skin tape for the pads.

## 12. Open items

| # | Item | Close with |
|---|---|---|
| B-1 | Closed 2026-11-09: U1 0.21, U10 0.22, U2 / U13 0.20, U12 EP 0.38 in the production paste Gerbers; confirm U12 against the AP6683 land pattern | – |
| B-2 | JLC: WLCSP 0.4 / DSBGA 0.35 placement, 0201 both sides on HDI, X-ray, nano-coated foil | JLC engineering |
| B-3 | Cell tab type and length (welded extension tab wanted) for the pod cell; leads and PCM on the case cell | Sharvi drawing / samples |
| B-4 | Dummy-cell soldering trial with a thermocouple at the seal | Bench, before the first pod |
| B-5 | Perimeter UV adhesive grade (skin-safe) and leak-test method | Supplier data, bench |
| B-6 | MSL and protective film on the MAXM86161; reflow bake | ADI data sheet |
| B-7 | Flex landing peel strength and hot-bar settings | Scrap flex |
| B-8 | B5819WS overload in a pogo short (236–329 mW vs 200–250 mW rated) | Firmware trip ≤ 50 ms after 80 mA, measured on the bench with a shorted dock |
| B-9 | Panel CPL: equal to the PCB tool's own export; confirm the WLCSP / DSBGA / LGA / DFN zero orientation in JLC's library | JLC CPL preview, before paying |
| B-10 | TP4054 heat: keep R5 4.7 k (199 mA, fold-back possible) or move to 6.8 k (147 mA) | Decide after the first case bring-up (thermocouple on U2) |
