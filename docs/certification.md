# Certification plan

## Open hardware: OSHWA

Het Bhavsar will self-certify Atrium with OSHWA as an individual ([requirements](https://certification.oshwa.org/requirements.html)), under the
standard certification, project type **Wearables**, country code IN. It is a wellness device, so the Open Healthware
track does not apply. Certification covers the openness of the design. It says nothing about safety or quality, and
the mark must never be used to suggest either.

| OSHWA requirement | Atrium | Status |
|---|---|---|
| Hardware under an open licence | CERN-OHL-S-2.0 (`LICENSE`) | ready |
| All software needed to operate it under an OSI licence | Pod and case firmware, phone app: Apache-2.0 | ready |
| Documentation under an open licence, no NC or ND | CC BY-SA 4.0 (`docs/LICENSE`) | ready |
| Original design files, publicly available | Schematics and boards as `.SchDoc` / `.PcbDoc` with PDFs; enclosure as STEP, STL and drawings | the repository is private until release |
| Design files in the preferred format for making changes | Boards: the native EasyEDA Pro project in `hardware/easyeda/`, plus Altium exports. Enclosure: STEP, which any CAD tool can edit; the parametric CAD scripts it was generated from are not published | ready for boards; STEP for the enclosure |
| Bill of materials | Every board's `production/*_BOM.csv`, with manufacturer part and LCSC numbers | ready |
| Assembly instructions (optional) | [`build-assembly-test.md`](build-assembly-test.md), [`quick-start.md`](quick-start.md) | ready |
| Third-party parts identified, with accessible datasheets | See below | ready |
| Hardware labelled with a version or release date | Bottom silkscreen: `ATRIUM EVT v0.1.0 2026-11` on the case board, `EVT v0.1` on the flex inside each pod (O-42) | ready |

**What is open and what is not.** Everything I made for Atrium (Het Bhavsar, publishing as Local Forge) is open: schematics, board layouts, Gerbers, enclosure
CAD, firmware, app, documentation and images. The parts on the boards are ordinary catalogue parts made by others,
each with a public datasheet reached through the LCSC number in the BOM: the nRF52832 radio SoC, the MAXM86161 optical
module, the LSM6DSOX motion sensor, the BQ21061 charger, the cell and its protection, the chip antenna, the CH32V003
case MCU and the passives. Their internal designs are not open, and OSHWA does not require them to be. The names
"Atrium" and "Local Forge" and their logos are not licensed (see the README).

**Before applying:**
1. Make the repository public.
2. Submit the form at [application.oshwa.org](https://application.oshwa.org/) as Het Bhavsar (individual), and register each later
   version under a new UID instead of overwriting this one.

## Product compliance (before any sale)

| Area | Standard / scheme | Applies to | Status |
|---|---|---|---|
| Radio, India | WPC ETA (2.4 GHz, licence-exempt) | pod | plan |
| Radio, EU / US | RED (EN 300 328, EN 301 489), FCC Part 15C | pod | plan |
| Electrical safety | IEC 62368-1 | pod, Plinth | plan |
| Lithium cells | IEC 62133-2; BIS IS 16046 (India) | pod cell | supplier reports |
| Transport | UN 38.3 | cells | supplier reports |
| Skin contact | ISO 10993-5 / -10 on the base, gasket, Skin and adhesive materials | pod | supplier reports, then a 7-day patch test |
| RF exposure | SAR or MPE exemption for low-power BLE | pod | plan |
| Materials | RoHS, REACH | all | BOM check |

Atrium is a wellness and research device, not a medical device. Keep its labelling and claims that way: no diagnosis, treatment or prevention claims.

Every line above is [VERIFY] against the current edition and notification before a test house is booked.
