# System architecture

How the Atrium pod is split into boards, how power and signals flow between them, and the design limits the EVT build is held to.

## Why 20.9 mm

The pod envelope is 20.9 × 11 × 5.3 mm (about 1.07 cm³), decided 2026-11-07. An earlier 19.4 mm envelope did not fit its parts: a footprint-accurate placement with real pads, 0.2 mm between parts, copper 0.2 mm from edges, the all-layer antenna zone, the base window, the flex landings and the clamp bands left about 20 radio-island parts and the BQ21061 without a valid position. At 20.9 mm all 64 parts place. The length is set by the chain along the radio-island bottom (inner edge, flex tongue, optical window, antenna zone), not by the crystal: at 20.4 mm with a 1.6 × 1.2 crystal every part places, but the antenna lands over the MAXM86161.

## Envelope and stack

| Item | Value |
|---|---|
| Envelope | 20.9 × 11 × 5.3 mm, stadium capsule |
| Z stack | 0.5 top wall + 0.2 swell gap + 4.0 cell + 0.1 SUS430 contact stiffener + 0.12 flex + 0.03 bond + 0.35 base = 5.3 mm |
| Base | 0.35 mm (thinned from 0.45 to make room for the stiffener) |
| Cell | 400909, 4.0 × 9 × 9 mm, 30 mAh |
| Islands | 6-layer HDI 1+4+1, 0.82 mm; 50 Ω CPWG 0.124 mm track / 0.127 mm gap |
| Flex | 2-layer polyimide, under the cell, no RF |

A cell 0.2 mm over nominal uses the whole swell gap (stack tolerance analysis in `hardware/simulation/`). Measure 10 cells before freezing the shell (open item O-11).

## Design decisions

| # | Decision | Why |
|---|---|---|
| D1 | Cell centred, two rigid islands, one flex jumper | A square cell can't go in a round end |
| D2 | Rigid 0603 chip antenna at the radio tip, all-layer keep-out | A flex antenna was rejected; RF stays off the flex |
| D3 | Antenna stays at the tip (side corner rejected) | The antenna datasheet asks for at least 5 × 3 mm of clearance. The tip gives about 7.6 mm² (about 50 %); a side corner gives about 6 mm² next to the cell. The tip is the lesser compromise. |
| D4 | VDD = 1.8 V everywhere | MAXM86161 I/O (VIH 1.4 V) and BQ21061 VDD are 1.8 V; the nRF52832 runs DC/DC down to 1.7 V |
| D5 | BQ21061 charger | I2C status and /PG, PMID power path, ship mode (10 nA) |
| D6 | AP6683 protection (integrated FET, 1 × 1 mm) | An independent protection layer for a cell sold without protection |
| D7 | Contacts on the flex, under the cell | Frees the power island. VIN at the centre with GND on both sides gives rotation-proof charging with no diode bridge, and the cell backs the pads against pogo force. |
| D8 | SWD on two contact pads plus a programming jig | A bricked pod can be recovered without opening it |
| D9 | NTC dividers: 10 k on top, NTC to GND | TH4 then needs one jumper line, not two. The firmware NTC conversion follows this divider. |
| D10 | Skin attachment: a hidden double-coated pad in a bed inside the pod's frame | The adhesive never shows and the electronics never touch glue. See [mechanical.md](mechanical.md). |
| D11 | Scale path: a custom 3.0–3.2 mm cell later | Gets the thickness to about 4.5 mm; keep the bay outline and tab positions stable |

## Not fixed yet (decided in EVT)

- Pi-match values.
- VLED cut-off voltage.
- HRV timing: the firmware runs at 25 sps with parabolic beat timing; the bench confirms it against ECG (O-18).
- Flex copper and polyimide thickness.
- Skin pad material and thickness.

## Block diagram

```
                       FLEX JUMPER (2-layer, under the cell, no RF)
  POWER ISLAND (x 0.6–4.16)  <== 10 pads ==  [contact pads on flex bottom]  == 10 pads ==>  RADIO ISLAND (x 13.86–20.3)
  ------------------------                 VIN (centre)  GND_A  GND_B                    ---------------------------
  BQ21061 charger  (0x6B) <--VIN--------------'                                           nRF52832-CIAA  (VDD 1.8 V)
    IN  <- VIN (+ESDA5 TVS)                SWDIO  SWDCLK ---------------------------->     SWD (47 R + ESD)
    PMID = VSYS ---------------------------------------------------------------------->   TPS7A0218 -> VDD_1V8
    BAT  <-> CELL+  (400909)                                                              MAXM86161A VLED <- VSYS
    VDD 1.8 V -> own VIO (BQ_VDD)                                                         MAXM86161A (0x62) on I2C
    /PG ----------------------------------------------------------------------------->   P0.14 (100 k pull-up)
  LSM6DSOX INT1 -------------------------------------------------------------------->    P0.12
  I2C SDA/SCL <======================================================================>   P0.15 / P0.16 (4.7 k pull-ups)
  U13 TPS7A0218 (VSYS -> VDD_IMU 1.8 V) -> LSM6DSOX (0x6A)                                TH4 NTC (radio top) -> P0.02/AIN0
  TH5 NTC -> BQ21061 TS (hardware charge-temp cut-off)                                    TH1 NTC (skin) -> P0.03/AIN1
  AP6683: CELL- <-> GND (integrated FET)                                                  VSYS/3.2 -> P0.28/AIN4
                                                                                          KH-1608-H08 antenna + pi-match
```

## Power tree

| Rail | Source | Loads | Notes |
|---|---|---|---|
| VIN (5 V) | Plinth or case pogo, to the centre pad | BQ21061 IN | Present only when docked. The BQ21061 blocks reverse current, so the skin-side pad is dead on battery. |
| CELL | 400909 via AP6683 | BQ21061 BAT | The AP6683 cuts CELL− on a fault |
| VSYS (PMID) | BQ21061 | TPS7A0218 (both), MAXM86161 VLED | 4.5 V regulated while charging; about VBAT on battery. Ship mode disconnects it (10 nA). |
| VDD_1V8 | TPS7A0218 (25 nA Iq) | nRF52832, I2C pull-ups, NTC dividers | |
| VDD_IMU | U13 TPS7A0218 from VSYS (25 nA Iq) | LSM6DSOX | Own LDO on the power island. BQ21061 VDD is rated only as the charger's I/O supply (SLUSDU0A 7.3.3), so it feeds only U10 VIO (BQ_VDD, 4.7 µF). |

## nRF52832 pin map

| Pin | Ball | Net | Notes |
|---|---|---|---|
| P0.15 | H4 | I2C_SDA | TWIM0, 400 kHz, 4.7 k pull-ups |
| P0.16 | H3 | I2C_SCL | |
| P0.18 | H2 | PPG_INT | MAXM86161A INTB, open-drain, internal pull-up |
| P0.20 | G2 | PPG_LDO_EN | High = MAXM86161A on; a 1 M pull-down holds it off in reset |
| P0.12 | H6 | IMU_INT1 | Push-pull, **no pull** (INT1 must be low or floating at IMU power-up) |
| P0.14 | H5 | CHG_PG | BQ21061 /PG, low when docked. Wakes the pod from System OFF on the Plinth; a lift opens the 60 s pairing window |
| P0.17 | G3 | NTC_EN | Drives the top of both NTC dividers for about 0.5 ms per reading: every second when worn or docked, every 10 s in Sleep and off-body |
| P0.03/AIN1 | D6 | TH1_SENSE | Skin NTC |
| P0.02/AIN0 | C6 | TH4_SENSE | Top NTC (radio island top, beside R11) |
| P0.28/AIN4 | A3 | VSYS_SENSE | 2.2 M / 1 M + 100 nF |
| SWDIO / SWDCLK | G1 / F1 | to the flex pads | 47 R + ESD5V3U04 |
| P0.21/nRESET | H1 | TP1 | Bench only |
| ANT | D1 | 3.3 nH / 1.0 pF match, CPWG, pi-match, antenna | Nordic reference match |

The firmware board definition in `firmware/boards/atrium/atrium_pod/` mirrors this table, and the pin map in `hardware/design-data/` is kept identical to it. Pins may be swapped during layout for fan-out, but analog nets must stay on AIN-capable pins, and the netlist, the devicetree and this table change together.

## Contacts (base, skin side)

ENIG pads, Ø1.0 mm, on the flex bottom, exposed through Ø1.2 mm holes in the 0.35 mm base:

| Pad | Pod coordinates (mm) | Used by |
|---|---|---|
| VIN | (10.45, 5.50), pod centre | Plinth, case, jig |
| GND_A / GND_B | (8.25, 5.50) / (12.65, 5.50), VIN ± 2.2 | Plinth (one pin, at GND_B), case, jig |
| SWDIO / SWDCLK | (6.70, 3.30) / (6.70, 7.70) | Programming jig and charging case |

A 180° turn maps VIN onto itself and GND_A onto GND_B, so the Plinth needs one VIN pin and one GND pin and no diode bridge.

Risk: the SWD pads face the skin. The nRF52832 SWD pins have weak internal pulls, so sweat can carry µA and slowly corrode the pads. Tested with artificial sweat in EVT (O-12).

## Wake and pairing (no buttons, no LEDs on the pod)

- On skin: the off-body green probe (5 mA, 8 Hz) sees reflected light for 2 s, then a pulse within 15 s, and the pod starts measuring in the chosen mode.
- On the Plinth: /PG goes low, the pod charges, and the Plinth light line breathes.
- Lifted off the Plinth: /PG rises and the pod accepts a new pairing for 60 s (also for 60 s after boot). It advertises at all times for bonded phones.
- Ship mode (factory): BQ21061 ship mode; the first time on a Plinth or in the case wakes it.

## Known compromises (accepted)

- Antenna clearance is about 50 % of the datasheet minimum. Mitigation: pi-match plus VNA tuning; no range claims until O-14 is measured.
- The SWD pads face the skin (O-12).
- The RC LFCLK costs a few µA more than a 32 kHz crystal; there is no room for the crystal.
