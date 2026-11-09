# 50 ohm CPWG feed width (L1 signal, L2 solid GND)

Closed form, no copper thickness, no solder mask. Use as a starting point, then JLC's calculator.

| Prepreg | h (mm) | er | gap (mm) | width for 50 ohm (mm) | eeff | buildable? |
|---|---:|---:|---:|---:|---:|---|
| 1080 | 0.0764 | 3.91 | 0.1 | 0.156 | 2.95 | yes |
| 1080 | 0.0764 | 3.91 | 0.127 | 0.161 | 3.00 | yes |
| 1080 | 0.0764 | 3.91 | 0.15 | 0.163 | 3.03 | yes |
| 3313 | 0.0994 | 4.1 | 0.1 | 0.189 | 3.00 | yes |
| 3313 | 0.0994 | 4.1 | 0.127 | 0.196 | 3.06 | yes |
| 3313 | 0.0994 | 4.1 | 0.15 | 0.200 | 3.09 | yes |
| 2116 | 0.1164 | 4.16 | 0.1 | 0.213 | 3.00 | yes |
| 2116 | 0.1164 | 4.16 | 0.127 | 0.222 | 3.06 | yes |
| 2116 | 0.1164 | 4.16 | 0.15 | 0.227 | 3.10 | yes |

## Recommendation

- Start with the 3313 row: about 0.20 mm trace, 0.127 mm (5 mil) gap, via fence both sides at <= 1 mm pitch (lambda/20 in FR-4 is ~3 mm; denser is fine).
- Keep the feed under 3 mm long, on L1, no vias, arc bends with radius >= 3x width (house rule).
- Guided wavelength in this line: lambda_g = c / (f * sqrt(eeff)) = 70.3 mm, so a 3 mm feed is ~15 deg of phase: the pi-match absorbs it.
- At this scale 35 um copper is ~1/4 of the gap, so real Z0 is a few ohm lower than this zero-thickness result; expect JLC's calculator to land nearer 0.15-0.18 mm. Trust the calculator, not this table.
- Solder mask over the line lowers Z0 a little; JLC can quote with or without mask. Open the mask over the RF line if the calculator says you're under 47 ohm.