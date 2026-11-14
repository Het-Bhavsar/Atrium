# Certification

## Open hardware: OSHWA

<a href="https://certification.oshwa.org/in000108.html"><img src="images/oshw-certification-mark-IN000108.svg" alt="OSHW certification mark, UID IN000108" width="220"></a>

Atrium is certified open source hardware, **OSHWA UID IN000108**, certified on 14 November 2026 for version 0.1.0-evt
([listing](https://certification.oshwa.org/in000108.html)). Het Bhavsar self-certified it as an individual
([requirements](https://certification.oshwa.org/requirements.html)), under the standard certification, project type
**Wearables**, country India. It is a wellness device, so the Open Healthware track does not apply. Certification
covers the openness of the design. It says nothing about safety or quality, and the mark must never be used to suggest
either.

Use the mark from `images/oshw-certification-mark-IN000108.svg` as OSHWA supplies it (graphite #333333 and coral
#FF4444, or one colour in black, white or grey), and do not change it. In plain text, write:
`[OSHW] IN000108 | Certified open source hardware | oshwa.org/cert`. Usage rules:
[certification.oshwa.org/mark-usage.html](https://certification.oshwa.org/mark-usage.html).

| OSHWA requirement | Atrium | Status |
|---|---|---|
| Hardware under an open licence | CERN-OHL-S-2.0 (`LICENSE`) | met |
| All software needed to operate it under an OSI licence | Pod and case firmware, phone app: Apache-2.0 | met |
| Documentation under an open licence, no NC or ND | CC BY-SA 4.0 (`docs/LICENSE`) | met |
| Original design files, publicly available | Schematics and boards as `.SchDoc` / `.PcbDoc` with PDFs; enclosure as STEP, STL and drawings | met: public since 9 November 2026 |
| Design files in the preferred format for making changes | Boards: the native EasyEDA Pro project in `hardware/easyeda/`, plus Altium exports. Enclosure: STEP, which any CAD tool can edit; the parametric CAD scripts it was generated from are not published | met: native project for the boards; STEP for the enclosure |
| Bill of materials | Every board's `production/*_BOM.csv`, with manufacturer part and LCSC numbers | met |
| Assembly instructions (optional) | [`build-assembly-test.md`](build-assembly-test.md), [`quick-start.md`](quick-start.md) | met |
| Third-party parts identified, with accessible datasheets | See below | met |
| Hardware labelled with a version or release date | Bottom silkscreen: `ATRIUM EVT v0.1.0 2026-11` on the case board, `EVT v0.1` on the flex inside each pod (O-42) | met |

**What is open and what is not.** Everything I made for Atrium (Het Bhavsar, published through the Local Forge open-source community) is open: schematics, board layouts, Gerbers, enclosure
CAD, firmware, app, documentation and images. The parts on the boards are ordinary catalogue parts made by others,
each with a public datasheet reached through the LCSC number in the BOM: the nRF52832 radio SoC, the MAXM86161 optical
module, the LSM6DSOX motion sensor, the BQ21061 charger, the cell and its protection, the chip antenna, the CH32V003
case MCU and the passives. Their internal designs are not open, and OSHWA does not require them to be. The names
"Atrium" and "Local Forge" and their logos are not licensed (see the README).

**Later versions.** The certificate names version 0.1.0-evt. Releases since then (v0.1.1-evt to v0.1.3-evt) keep the
same licences and publish the same kinds of files. Register a later version that changes the hardware materially under
a new UID at [application.oshwa.org](https://application.oshwa.org/) instead of editing this one.

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
