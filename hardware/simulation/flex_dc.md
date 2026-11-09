# Flex jumper DC: line resistance and IR drop

12 um copper, 1.43 mOhm/sq. Sum of drawn copper per net (all branches; a series path is shorter), plus 2 mOhm per signal via.

| Net | copper length (mm) | min width (mm) | R (mOhm) | I (mA) | drop (mV) |
|---|---:|---:|---:|---:|---:|
| CHG_PG | 10.7 | 0.10 | 153.0 | 1 | 0.15 |
| GND | 9.8 | 0.14 | 65.9 | 150 | 9.89 |
| I2C_SCL | 12.7 | 0.10 | 182.2 | 1 | 0.18 |
| I2C_SDA | 12.8 | 0.10 | 183.2 | 1 | 0.18 |
| IMU_INT1 | 10.7 | 0.10 | 154.0 | 1 | 0.15 |
| SWDCLK | 13.7 | 0.10 | 198.7 | 1 | 0.20 |
| SWDIO | 14.9 | 0.10 | 218.1 | 1 | 0.22 |
| VIN | 6.8 | 0.30 | 34.4 | 100 | 3.44 |
| VSYS | 10.7 | 0.30 | 51.0 | 150 | 7.65 |

- GND return also has the L2 pour: about 1.5 squares end to end = 2.2 mOhm in parallel with the L1 GND lines, so the GND numbers above are an upper bound.
- VIN: the BQ21061 input needs > 4.35 V with 5 V from the Plinth; tens of mV on the flex are irrelevant.
- VSYS to the radio island: the drop at 150 mA peak is well inside the nRF52832 / LDO headroom.

## S-bend radii (static, single copper layer)

- power: run 0.85 mm, rise 0.80 mm -> r 0.426 mm = 6.1 x t (t ~0.07 mm in the bend) [VERIFY JLC static bend radius]
- radio: run 0.76 mm, rise 0.80 mm -> r 0.380 mm = 5.4 x t (t ~0.07 mm in the bend) [VERIFY JLC static bend radius]
