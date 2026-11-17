# I2C pull-ups (1.8 V, 400 kHz)

| Contributor | pF |
|---|---:|
| nRF52832 pin | 4.0 |
| MAXM86161 pin (ASSUMPTION) | 5.0 |
| LSM6DSV16BX pin (ASSUMPTION) [VERIFY DS13771 pin capacitance] | 5.0 |
| BQ21061 pin (ASSUMPTION) | 5.0 |
| Rigid traces, ~15 mm @ ~1 pF/cm | 1.5 |
| Flex jumper, ~12 mm over GND | 2.0 |
| Margin | 7.5 |
| **Bus total** | **30.0** |

- R_min (VOL = 0.36 V at 3 mA): 480 ohm
- R_max (tr <= 300 ns): 11802 ohm
- Chosen: 4.7 kohm on SDA and SCL, radio island, to VDD_1V8. tr = 119 ns.
- Each line costs 383 uA only while held low, so average cost is set by bus traffic (in power_budget).
- MAXM86161 INTB and BQ21061 PG are open-drain: INTB uses the nRF internal pull-up; PG gets 100 kohm to VDD_1V8 (18 uA, only while on the Plinth).
- LSM6DSV16BX INT1 is push-pull; keep it low or floating at IMU power-up (ST note for the family, [VERIFY DS13771]), so the nRF input on P0.12 has NO pull.