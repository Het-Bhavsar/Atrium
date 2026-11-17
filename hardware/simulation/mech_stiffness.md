# Contact-stack mechanics under the pogo pins

Stiffener 8.55 x 8.10 x 0.1 mm SUS430 (E 200 GPa), on four pins at (5.7, 2.025), (5.7, 8.975), (12.35, 2.025), (12.35, 8.975). Pogo 0.3 N per pin at working stroke.
Plate model: free edges, point supports at the pins only (flex, tape and cell ignored: conservative).

| Load case | Max deflection | At the pads | Peak stress | Stress / yield | Max pin pull |
|---|---:|---|---:|---:|---:|
| Plinth (VIN + GND_B) | 52.1 um | VIN 37.8 um, GND_B 48.0 um | 127 MPa | 62 % | 0.264 N |
| Case / jig (VIN, GND_B, SWDIO, SWDCLK) | 55.8 um | VIN 54.1 um, GND_B 54.5 um, SWDIO 28.3 um, SWDCLK 28.3 um | 143 MPa | 70 % | 0.309 N |

## Pins, glue and base

- Pin D0.45 in a D0.6 hole: 0.075 mm radial glue gap over 0.21 mm (flex + steel).
- Glue: 0.297 mm2 per pin x 5 MPa = 1.48 N per pin against 0.309 N: safety 4.8x.
- Pin root (SLA resin, 30 MPa derated): 4.77 N per pin: safety 15.4x.
- Base strip 2.0 mm wide x 0.35 mm, 1.47 mm from pin to rim: stress 11.2 MPa (resin flexural ~60 MPa), deflection 18.5 um.

## Plinth magnets against the pogo lift (estimate, bench-verify)

| Magnet | Steel | Gap | Pull, ideal | Pull, thin-steel limited | Two magnets | Plinth pogo lift | Net |
|---|---|---:|---:|---:|---:|---:|---:|
| D2.5 x 1.5 N52 (design) | 0.10 SUS430 | 0.82 | 0.23 N | 0.20 N | 0.39 N | 0.60 N | pod floats |
| D3 x 2 N52 | 0.10 SUS430 | 0.82 | 0.48 N | 0.20 N | 0.39 N | 0.60 N | pod floats |
| D2.5 x 1.5, floor 0.3 thinner | 0.10 SUS430 | 0.52 | 0.45 N | 0.20 N | 0.39 N | 0.60 N | pod floats |

## Reading it

- The stiffener is stiff enough: tens of microns of deflection at the pads against about 1 mm of pogo travel, and stress well below yield.
- Each glued pin carries about a quarter of the pogo load; glue and resin both have several times that strength.
- The magnets are the weak link. A 0.10 mm steel sheet saturates near a small magnet, so two D2.5 x 1.5 discs at 0.82 mm probably pull less than two 0.3 N pogos push. In the Plinth the pod could float on its pins instead of seating. Fix before the Plinth is built: pogo pins of about 0.1 N, or stronger magnets / a thinner nest floor, and measure the pull on the bench. If JLC's steel is SUS304 rather than SUS430, the pull is close to zero. The charging case is not affected: its clips and lid hold the pod.
