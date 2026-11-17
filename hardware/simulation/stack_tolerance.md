# Tolerance stacks

| Stack | nominal | worst case | RSS | slack (flexible gaps) | P(rigid parts exceed envelope) |
|---|---:|---:|---:|---:|---:|
| Z through cell (env 5.45) | 5.45 | 5.75 | 5.58 | 0.17 | 0.01% |
| X long axis (env 20.9) | 20.90 | 21.30 | 21.09 | 0.80 | 0.00% |

## Reading it

- Z: the 0.165 swell air is what a thicker cell or flex eats first; a cell 0.165 mm over its measured 4.1 uses all of it. That's why the bay is specified for 'any 400909' and why you measure 10 cells before freezing CAD.
  Cells above 4.2 mm are rejected at incoming (O-55); take more from the swell gap only after a 300-cycle swell test, never from the walls.
- X: the measured 9.1 mm cell leaves 0.2 mm bay slack plus two 0.2 mm gaps. A cell at 9.3 mm still fits; at 9.5 mm it hits an island.
- Flex: JLC offers no 12.5 um core; its thinnest 2-layer flex is 0.11 mm +-0.05 (25 um PI). The 0.02 over the old 0.12 slot came out of the swell air (0.20 -> 0.18).