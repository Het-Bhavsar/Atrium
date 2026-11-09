# Tolerance stacks

| Stack | nominal | worst case | RSS | slack (flexible gaps) | P(rigid parts exceed envelope) |
|---|---:|---:|---:|---:|---:|
| Z through cell (env 5.3) | 5.30 | 5.65 | 5.51 | 0.20 | 0.26% |
| X long axis (env 20.9) | 20.90 | 21.40 | 21.15 | 0.90 | 0.00% |

## Reading it

- Z: a +0.2 mm cell eats the whole swell allowance. That's why the bay is specified for 'any 400909' and why you measure 10 cells before freezing CAD.
  If cells measure above 4.05 mm, take it from the swell gap only after a 300-cycle swell test, never from the walls.
- X: the 0.3 mm bay slack plus two 0.2 mm gaps absorb a +0.2 mm cell. A cell at 9.2 mm is fine; at 9.4 mm it hits an island.
- Both stacks leave zero room for a thicker jumper. Order the flex as 2-layer, 1/3 oz Cu, 12.5 um PI, no adhesive (adhesiveless) if JLC offers it [VERIFY].