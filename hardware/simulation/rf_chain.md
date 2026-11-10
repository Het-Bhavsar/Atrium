# RF chain as drawn: insertion loss and harmonic rejection (50 ohm ports)

Line: 52.7 ohm, eeff 3.05 (CPWG field model). Segments (mm): ANT_NRF 0.57, C3->L3 0.54, L3->node 0.24, node->Z41 0.43, Z41->feed 0.89. Pi default: C42 / C40 DNP (pad 0.05 pF), Z41 0 ohm (0201, 0.35 nH). Part parasitics are ASSUMPTIONS [VERIFY].

| Frequency | S21 (dB) | S11 (dB) |
|---|---:|---:|
| BLE low 2.402 GHz | -1.31 | -6.8 |
| BLE mid 2.440 GHz | -1.38 | -6.6 |
| BLE high 2.480 GHz | -1.45 | -6.4 |
| H2 4.880 GHz | -10.79 | -0.6 |
| H3 7.320 GHz | -27.12 | -0.2 |

- Copper alone (no Nordic C3/L3) loses 0.06 dB at 2.44 GHz: the board traces are not the loss budget.
- With 50 ohm on both ports the Nordic C3/L3 shows as a mild mismatch; on the real chip it transforms the PA's optimum load to 50 ohm (nRF52832 PS reference circuitry). Its job here is also the low-pass: compare H2/H3 rows.
- Conducted harmonics and TX power are measured at the pi pads with a pigtail (EVT bring-up); the pi is tuned on the VNA with a Smith-chart match in the closed pod.
- Order since 2026-11-10: C3 (1.0 pF) shunts the ANT ball's node to ground, then L3 (3.3 nH) runs in series, as in the nRF52832 CIAA DC/DC reference (PS v1.8, section 53.5, Figure 175). Before that change the board had L3 first and C3 after it.
