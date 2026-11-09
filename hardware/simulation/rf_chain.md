# RF chain as drawn: insertion loss and harmonic rejection (50 ohm ports)

Line: 52.7 ohm, eeff 3.05 (CPWG field model). Segments (mm): ANT_NRF 0.57, L3->node 0.48, node->Z41 0.43, Z41->feed 0.89. Pi default: C42 / C40 DNP (pad 0.05 pF), Z41 0 ohm (0201, 0.35 nH). Part parasitics are ASSUMPTIONS [VERIFY].

| Frequency | S21 (dB) | S11 (dB) |
|---|---:|---:|
| BLE low 2.402 GHz | -1.02 | -8.5 |
| BLE mid 2.440 GHz | -1.08 | -8.3 |
| BLE high 2.480 GHz | -1.14 | -8.1 |
| H2 4.880 GHz | -10.11 | -1.1 |
| H3 7.320 GHz | -27.56 | -0.5 |

- Copper alone (no Nordic L3/C3) loses 0.05 dB at 2.44 GHz: the board traces are not the loss budget.
- With 50 ohm on both ports the Nordic L3/C3 shows as a mild mismatch; on the real chip it transforms the PA's optimum load to 50 ohm (nRF52832 PS reference circuitry). Its job here is also the low-pass: compare H2/H3 rows.
- Conducted harmonics and TX power are measured at the pi pads with a pigtail (EVT bring-up); the pi is tuned on the VNA with a Smith-chart match in the closed pod.
