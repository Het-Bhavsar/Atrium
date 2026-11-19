# Contact-stack mechanics under the pogo pins

Stiffener 8.35 x 7.90 x 0.1 mm SUS430 (E 200 GPa), on four pins at (5.7, 2.025), (5.7, 8.975), (12.35, 2.025), (12.35, 8.975). Pogo 0.3 N per pin at working stroke.
Plate model: free edges, point supports at the pins only (flex, tape and cell ignored: conservative).

| Load case | Max deflection | At the pads | Peak stress | Stress / yield | Max pin pull |
|---|---:|---|---:|---:|---:|
| Plinth (VIN + GND_B) | 54.3 um | VIN 39.1 um, GND_B 50.4 um | 135 MPa | 66 % | 0.264 N |
| Case / jig (VIN, GND_B, SWDIO, SWDCLK) | 58.6 um | VIN 56.0 um, GND_B 57.2 um, SWDIO 29.4 um, SWDCLK 29.4 um | 151 MPa | 74 % | 0.309 N |

## Pins, glue and base

- Pin D0.45 in a D0.6 hole: 0.075 mm radial glue gap over 0.21 mm (flex + steel).
- Glue: 0.321 mm2 per pin x 5 MPa = 1.61 N per pin against 0.309 N: safety 5.2x.
- Pin root (SLA resin, 30 MPa derated): 4.77 N per pin: safety 15.4x.
- Base strip 2.0 mm wide x 0.35 mm, 1.47 mm from pin to rim: stress 11.2 MPa (resin flexural ~60 MPa), deflection 18.5 um.

## Plinth magnets against the pogo lift (estimate, bench-verify)

| Magnet | Steel | Gap | Pull, ideal | Pull, thin-steel limited | Two magnets | Plinth pogo lift | Magnets alone |
|---|---|---:|---:|---:|---:|---:|---:|
| D2.5 x 1.5 N52 (design) | 0.10 SUS430 | 0.82 | 0.23 N | 0.20 N | 0.39 N | 0.60 N | pod floats |
| D3 x 2 N52 | 0.10 SUS430 | 0.82 | 0.48 N | 0.20 N | 0.39 N | 0.60 N | pod floats |
| D2.5 x 1.5, floor 0.3 thinner | 0.10 SUS430 | 0.52 | 0.45 N | 0.20 N | 0.39 N | 0.60 N | pod floats |

## Plinth retention with the snap clips (2026-11-17 fix)

| Clip material | Pull-out (two clips) | + magnets (thin-steel limited) | Pogo lift | Margin |
|---|---:|---:|---:|---:|
| SLA resin (E 2.5 GPa) | 4.3 N | 4.6 N | 0.6 N | 7.7x |
| PLA (E 3.5 GPa) | 6.0 N | 6.4 N | 0.6 N | 10.6x |

## Reading it

- The stiffener is stiff enough: tens of microns of deflection at the pads against about 1 mm of pogo travel, and stress well below yield.
- Each glued pin carries about a quarter of the pogo load; glue and resin both have several times that strength.
- Magnets alone can't hold the pod: a 0.10 mm steel sheet saturates near a small magnet, so two D2.5 x 1.5 discs pull about 0.4 N against 0.6 N of Plinth pogo lift (and close to nothing if the steel is SUS304).
- Fixed: the Plinth now has the charging case's two snap clips. They hold about 4-6 N against 0.6 N, whatever the steel grade; the magnets only centre the pod. Push-in is about 7-10 N, the same as the case.
