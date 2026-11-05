# Certification plan

## Open hardware: OSHWA

Atrium will apply for OSHWA certification (country code IN) at its first public release. The requirements and Atrium's status:

| Requirement | Status |
|---|---|
| Hardware licence | CERN-OHL-S-2.0 |
| Software licence | Apache-2.0 |
| Docs licence | CC BY-SA 4.0 |
| Source design files | KiCad / EasyEDA sources, CAD scripts and STEP |
| Bill of materials | `hardware/*/bom` |
| Build and assembly instructions | after EVT |
| Public repository | at release |

## Product compliance (before any sale)

| Area | Standard / scheme | Applies to | Status |
|---|---|---|---|
| Radio, India | WPC ETA (2.4 GHz, licence-exempt) | pod | plan |
| Radio, EU / US | RED (EN 300 328, EN 301 489), FCC Part 15C | pod | plan |
| Electrical safety | IEC 62368-1 | pod, case | plan |
| Lithium cells | IEC 62133-2; BIS IS 16046 (India) | both cells | supplier reports |
| Transport | UN 38.3 | cells | supplier reports |
| Skin contact | ISO 10993-5 / -10 on the adhesive and the window materials | pod | supplier reports, then a 7-day patch test |
| RF exposure | SAR or MPE exemption for low-power BLE | pod | plan |
| Materials | RoHS, REACH | all | BOM check |

Atrium is a wellness and research device, not a medical device. Keep its labelling and claims that way: no diagnosis, treatment or prevention claims.

Every line above is [VERIFY] against the current edition and notification before a test house is booked.
