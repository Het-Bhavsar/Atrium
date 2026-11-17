# Tolerance stacks

| Stack | nominal | worst case | RSS | slack (flexible gaps) | P(rigid parts exceed envelope) |
|---|---:|---:|---:|---:|---:|
| Z through cell (env 5.3) | 5.30 | 5.69 | 5.52 | 0.18 | 0.69% |
| X long axis (env 20.9) | 20.90 | 21.40 | 21.15 | 0.90 | 0.00% |

## Reading it

- Z: the 0.18 swell air is what a thicker cell or flex eats first; a +0.18 mm cell uses all of it. That's why the bay is specified for 'any 400909' and why you measure 10 cells before freezing CAD.
  If cells measure above 4.05 mm, take it from the swell gap only after a 300-cycle swell test, never from the walls.
- X: the 0.3 mm bay slack plus two 0.2 mm gaps absorb a +0.2 mm cell. A cell at 9.2 mm is fine; at 9.4 mm it hits an island.
- Flex: JLC offers no 12.5 um core; its thinnest 2-layer flex is 0.11 mm +-0.05 (25 um PI). The 0.02 over the old 0.12 slot came out of the swell air (0.20 -> 0.18).