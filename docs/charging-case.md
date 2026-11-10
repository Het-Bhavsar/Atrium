# Charging case

The portable case that carries one pod, charges it from its own cell, and shows the wearer what is going on. The Plinth (desk puck, see [mechanical.md](mechanical.md#plinth)) stays as the optional desk dock; the case is the charger that ships in the box.

Case cell: **Sharvi KP-502030, 3.7 V 500 mAh LiPo, 30 × 20 × 4 mm** as listed. The 502030 code reads 5.0 × 20 × 30, so the CAD reserves **5.0 mm** until a cell is measured. Enclosure CAD is in `mechanical/case/`; schematic, PCB and production files are in `hardware/case/charging-case/`; firmware is in `firmware/case/`.

## What it does

| Function | How |
|---|---|
| Charge the pod | 5 V on the pod's VIN contact through a current-limited switch; the pod's BQ21061 charges its 30 mAh cell at 15 mA |
| Pod charge status | The case measures the pod's current on a low-side shunt in the pogo GND return: > 5 mA = charging, < 2.5 mA for 30 s with the pod present = full. Presence: SWDIO reads high (the powered nRF52832 pulls it up [VERIFY]) against a 1 M pull-down on the pod side of the translator, or current > 0.4 mA. No data link needed. |
| Case battery status | The MCU runs from the cell, so it reads its own VDD through VREFINT (ADC ch 8, ±2.5 %, trimmed at test) and shows it on the LED when the lid opens or the button is pressed |
| Charge the case | USB-C 5 V, BQ24040 at 200 mA (0.4 C). Its TS input reads a 10 k NTC on the board above the cell and cuts or reduces the charge when the cell is too cold or too hot, with no MCU involved. USB also feeds the pod directly while plugged in |
| Reset the pod (soft) | The case toggles VIN in a pattern (3 × 200 ms off within 2 s); the pod firmware sees it on CHG_PG and reboots |
| Reset the pod (hard) | The case drives SWD (level-shifted to 1.8 V) and writes the nRF52832 **CTRL-AP RESET** register. This works with a hung app and with APPROTECT on (no erase). It is also a recovery path: the case can mass-erase, and the app could push firmware later. |
| Wake the pod from ship mode | VIN plug-in exits BQ21061 ship mode (datasheet behaviour), so docking wakes a shipped pod |

Design choices:
- **Keyed nest.** The pod sits one way round (window end to the hinge) so the case can reach the SWD pads. The Plinth stays rotation-proof (VIN and GND only). Four pogo pins at the programming-jig positions in the pod frame: VIN (10.45, 5.50), GND (12.65, 5.50), SWDIO (6.70, 3.30), SWDCLK (6.70, 7.70).
- **Retention.** As in the Plinth: two Ø2.5 × 1.5 N52 discs under the nest floor at the pod centre ± 2.55 mm in y, pulling the SUS430 stiffener; pogos ≤ 0.3 N each at working stroke. With 4 pins the magnet pull would have to beat 1.2 N, so the lid carries a soft pad that holds the pod on the pins when closed (foam sizing in [mechanical.md](mechanical.md#charging-case-enclosure)).
- **One LED** (RGB, light pipe to the front) and **one recessed button**. A hall sensor sees the lid magnet.

## LED and button

| State | LED |
|---|---|
| Lid opened, pod charging | Amber, slow breathe, 5 s |
| Lid opened, pod full | Green, steady 5 s |
| Lid opened, no pod | Case battery: 3 green blinks > 60 %, 2 amber 20–60 %, 1 red < 20 % |
| Pod contact fault (over-current trip) | Red fast blink, VIN off, retry every 30 s |
| USB plugged in, case charging / full | Amber steady / green steady (pod status takes priority while the lid is open) |
| Case battery < 3.4 V | Stop feeding the pod, red blink on lid open |

| Button | Action |
|---|---|
| Short press | Show pod status, then case battery |
| Hold 5 s (pod docked) | Soft reset (VIN pattern); white LED flash |
| Hold 12 s (pod docked) | Hard reset through SWD CTRL-AP; white double flash |

## Electronics

One 2-layer 0.8 mm board under the nest.

```
USB-C (CC 5.1k) ─ VBUS ─┬─ BQ24040 (200 mA, TS) ─ CELL+ (KP-502030, PCM in pack [VERIFY]) ─┬─ CH32V003 VDD (2.7-5.5 V direct)
                        │                                                              ├─ TPS61023 boost 5.2 V (EN from MCU)
                        └─ 1N5819WS ┐                                      1N5819WS ───┘
                                   └── 5V_POD (diode-OR) ─ SY6280 (EN, 0.4 A ISET backstop) ─ pogo VIN
pogo GND ─ 1 Ω shunt (low side) ─ GND     OPA (CH32V003 plain op-amp, external gain: Rf 20 k / Rg 1 k = x21; OPA_P via
                                          10 k from the shunt + 1.5 M from VDD = 22-28 mV pedestal for the ±13 mV offset,
                                          zeroed with VIN off before each feed) -> OPO = ADC A7; full scale ~120 mA
SWD: CH32V003 GPIO ─ 2 × 74LVC1T45 (VCCA = VDD, VCCB = 1.8 V from XC6206P182MR fed by GPIO PD5, Ioff keeps the pod side
     unpowered when the LDO is off) ─ 100 Ω ─ pogo SWDIO / SWDCLK; 1 M pull-down on SWDIO B side (presence)
HAL248 (lid) ─ PC2 wake;  button ─ PC3 wake;  RGB LED (cathodes on TIM2 PWM, anodes to VDD, ~2 mA);  BQ24040 CHG ─ PD6
VBUS_DET ─ PC1 (10 k / 100 k);  cell NTC 100 k B3950 / 100 k from PD0 ─ PC4 (A2);  TH2 10 k NTC (board, above the cell) ─ BQ24040 TS;  SWIO pad (PD1) + VDD + GND for flashing
```

Key parts (LCSC numbers checked 2026-11-08; re-check stock before ordering):

| Ref | Part | LCSC | Note |
|---|---|---|---|
| U1 | CH32V003F4U6, RISC-V MCU, QFN-20 | C5299908 | Runs straight from the cell; internal OPA for the shunt [VERIFY OPA pins] |
| U2 | BQ24040DSQR linear charger with a battery-temperature input, WSON-10 2 × 2 mm | C81080 | R5 2.7 k on ISET: 540 / 2700 = 200 mA (187–210 mA with K_ISET 510–560 AΩ and 1 % R, SLUS941H). R24 2 k on PRE-TERM: 10 % termination, 20 % pre-charge. TS to TH2 (10 k NTC, NCP15XH103F03RC C77131) on the board above the cell, 3.0 mm from U2 (build item B-10): JEITA window, charge stops at the limits. 10 h safety timer, input over-voltage cut-off. CHG to PD6; PG and NC open; thermal pad to GND. Replaces the TP4054, which had no temperature input |
| U3 | TPS61023DRLR boost, SOT-563 (DRLT on a larger reel) | C919459 | 5.2 V out, PFM at light load; EN from the MCU [VERIFY load disconnect] |
| U4 | SY6280AAC current-limited switch, SOT-23-5 | C55136 | ISET for the 0.4 A minimum; EN from the MCU, toggles VIN for the soft-reset pattern |
| U5, U6 | 74LVC1T45 level translator | [VERIFY LCSC] | SWDCLK fixed direction, SWDIO with DIR from the MCU |
| U7 | XC6206P182MR 1.8 V LDO, SOT-23 | C21659 | VCCB of the translators, enabled only while the pod is powered |
| U8 | HAL248 SW omnipolar hall, 1.8–5.5 V, 3 µA | C6320892 | Lid sensor |
| J1 | USB-C 16-pin receptacle | C2765186 | 5.1 k CC pull-downs, VBUS TVS |
| D1, D2 | 1N5819WS Schottky, SOD-323 | C191023 | Diode-OR of VBUS and boost (SOD-323 because two SOD-123 bodies did not fit between SW1 and J1) |
| BT1 | Sharvi KP-502030 500 mAh | Sharvi | KP-502030 packs usually ship with a PCM and red / black bare wire leads [VERIFY on the delivered cell]. Each lead goes up through a plated 0.8 mm hole (BT1 + at (−15.5, +2.0), BT2 − at (−15.5, −2.0), past the cell end, 1.3 mm pads both sides) and is soldered on top; trim with a small loop, add a dab of glue under the board for strain relief; foam pad under the cell |
| P1–P4 | SMD pogo pins, 1.0–1.5 mm stroke, ≤ 0.3 N | [VERIFY] | Through the nest floor to the pod pads |

Budgets:
- Pod full charge from the case: 30 mAh × 4.2 V / (boost 0.88 × BQ21061 linear 0.76) ≈ 0.19 Wh ≈ 50 mAh of case cell, so **about 8 pod charges per case charge** (80 % usable), or 1–2 months of wear.
- Standby (lid closed, no USB): MCU standby about 10 µA + hall 3 µA + BQ24040 battery leakage with no input [VERIFY SLUS941H, about 1 µA] + 1 M pull-downs ≈ 15–20 µA, which gives years of shelf life. The OPA (273 µA) runs only while feeding.
- Case charge: 500 mAh at 200 mA plus the constant-voltage tail ≈ 3 h [EST].
- Thermal: the pod rises about 9 °C over ambient while charging (thermal analysis in `hardware/simulation/`), so the lid has vents beside the nest. The case stops feeding the pod above 45 °C case-cell temperature (cell NTC on the board [VERIFY pack NTC]).

## PCB

- **Outline:** 33 × 22 mm, R 3.5 corners, in the case frame (origin at the centre, +X toward the USB-C, +Y toward the hinge). The outline DXF is `mechanical/case/DXF/case_pcb_outline.dxf`. The cell's wire leads use two plated 0.8 mm holes at x −15.5 (an earlier 1 × 8 mm tab slot there was removed 2026-11-08).
- **2 layers, 0.8 mm** (decided 2026-11-08 after a 4-layer study): FR-4 0.71 mm core, 1 oz copper. Top: all parts, signals, battery trunk and power. Bottom: no parts (it faces the cell), a GND pour plus ordered signal corridors. JLC 2-layer rules: track / space 0.15, vias 0.20 / 0.45 [VERIFY JLC 2-layer surcharge for 0.2 mm holes], copper 0.3 from the edge, pours 0.20 clear and joined to pads solid (reflow only, so no thermal spokes).
- **Silkscreen:** a reference designator is printed only where it fits beside its own part, upright, clear of pads, holes and the edge (0.8 mm characters, 0.66 mm where space is short). The board is too dense for all of them; the rest are in the PCB PDF and the interactive BOM, and assembly uses the CPL file.
- **Routing** is hand-drawn, segment by segment, and checked for clearance, edge distance and per-net connectivity including the pours. The MCU pin map is fixed by the firmware, so its escapes are sorted by layer: top-row and right-column pins drop to the bottom and run in two ordered bundles (east: BTN, VBUS_DET, VIN_EN; west: LED_G, LID). The SWD A-side runs west on the bottom to the translators, which are turned so their B pins face the pogo pads. The battery trunk (0.4 mm) runs on top from BT1 under U5, south of the pogo pads, and up to the charger and boost. Boost hot loop: U3 VOUT to C4 (0.8 mm) with GND through the bottom pour.
- **Result (2026-11-08):** 113 planned tracks (263 segments), 80 vias (35 GND stitching), GND pours on both layers. DRC after refill: 0 connection errors and 0 clearance errors, except 2 inside the USB-C footprint (its GND pads sit 0.17 mm from its own shell slots; the maker's land pattern, accepted). Gerbers rendered and checked; the board STEP was used for the enclosure fit check.
- **Placement** is by hand and checked for edge 0.3, courtyards 0.2, magnet / pogo / clamp keep-outs and the nest zone (≤ 1.0 mm tall). Exception: D3 (LED) is fixed by the enclosure at (−9, −10); its pads are 0.25 from the edge.
- SW1 is the Alps SKSCLAE010 side-push (C130445, 3.5 × 2.9 × 1.25), stem toward −Y; its symbol joins pins 1–2 and 3–4 (BTN on 1 / 2, GND on 3 / 4).
- Production files: `Atrium_Charging_Case_gerber.zip`, `Atrium_Charging_Case_BOM.csv` and `Atrium_Charging_Case_CPL.csv` in `hardware/case/charging-case/production/`.

## MCU pin map

CH32V003F4U6, QFN-20, ch32fun toolchain.

| Pin | Port | Net | Note |
|---|---|---|---|
| 0 | EP | GND | |
| 1 | PD7 | LED_B | TIM2_CH4, sinks; NRST disabled in the option byte at first flash |
| 2 | PA1 | OPA_N | Rf 20 k from PD4, Rg 1 k to GND |
| 3 | PA2 | OPA_P | 10 k from the shunt, 1.5 M from VDD |
| 4 | VSS | GND | |
| 5 | PD0 | VIN_EN | SY6280 EN, top of the NTC divider; 1 M to GND |
| 6 | VDD | VDD (cell) | 100 n + 1 µ |
| 7 | PC0 | LED_G | TIM2_CH3, sinks |
| 8 | PC1 | VBUS_DET | 10 k from VBUS, 100 k to GND, EXTI wake |
| 9 | PC2 | LID | HAL248 output, EXTI wake |
| 10 | PC3 | BTN | To GND, internal pull-up, EXTI wake |
| 11 | PC4 | NTC_ADC | A2 |
| 12 | PC5 | SWDCLK_A | U5 A (DIR tied to VCCA) |
| 13 | PC6 | SWDIO_A | U6 A |
| 14 | PC7 | SWDIO_DIR | U6 DIR (high = MCU drives) |
| 15 | PD1 | SWIO | Case programming pad |
| 16 | PD2 | BOOST_EN | TPS61023 EN, 1 M to GND |
| 17 | PD3 | LED_R | TIM2_CH2, sinks |
| 18 | PD4 | OPA_OUT | OPO, ADC A7 |
| 19 | PD5 | LDO_PWR | Feeds XC6206 VIN, 1 M to GND |
| 20 | PD6 | CHRG | BQ24040 CHG (open drain), internal pull-up |

HAL248 output type [VERIFY]: if open-drain, fit the 4.7 M pull-up (DNP otherwise).

## Firmware

**Firmware defaults** (adjustable): after full, VIN off with a top-up every 4 h (VIN stays on while USB is plugged in); lid open turns VIN on to probe and keeps it on while open; hold actions fire on release (5–12 s soft reset, ≥ 12 s hard reset, a white flash marks each threshold); feeding stops above 45 °C and resumes below 42 °C; low cut-off 3.4 V, resume at 3.6 V or on USB; over-current in firmware, retry after 30 s: a fast cut every 20 ms frame (two readings over 80 mA, from 10 ms after each VIN-on edge, pulse edges included: within 50 ms of the edge), and a slow backstop (3 tick readings over 80 mA, counted from the second tick after VIN on).

**Case VIN signature** (decided 2026-11-08): 3 s after the case starts feeding a pod, and again after each reset, the case drops VIN twice for 200 ms, 300 ms apart. The pod counts two drop-outs of 100–400 ms within 2 s as "fed by the case" (Charge State byte 4 bit 4, see [ble_gatt.md](ble_gatt.md)) and clears the bit when VIN is gone for more than 400 ms; three drop-outs within 2 s remain the soft reset. The case and the Plinth both supply only VIN, so without this the pod can't tell them apart. No hardware impact: the SY6280 EN already toggles VIN.

**Case (CH32V003, C, no RTOS):** the state machine above, OPA + ADC current measurement at 10 Hz while feeding, standby with hall / button / USB wake, and an SWD host (bit-banged, about 1 MHz) that only does DP / CTRL-AP access: read IDR, write RESET, and later ERASEALL.

**Pod side:** a CHG_PG edge handler that counts VIN drop-outs (soft reset), BQ21061 ICHG = 15 mA on every VIN-good, and a BLE status bit "on case". The case never powers the 1.8 V translator side before the pod's VIN has been on for 100 ms, so it never back-powers an unpowered nRF52832 through its pins.

## Open items

- Measure the KP-502030: thickness, lead position, PCM present, NTC wire.
- Pogo part, stroke and force; magnet pull with 4 pins.
- Light-pipe and LED position after the enclosure layout.
- BIS and UN38.3 papers for the case cell from the cell supplier.

The full list is O-40 in [open-items.md](open-items.md) and B-8 / B-10 in [build-assembly-test.md](build-assembly-test.md#12-open-items).
