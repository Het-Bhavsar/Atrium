# BLE link budget (pod on the temporal region -> phone)

Uniform Monte Carlo over the ranges below. Positive margin = link closes.

| Term | range |
|---|---|
| TX power (nRF52832 max +4 dBm) | 4 to 4 |
| Pod antenna efficiency+gain on head (dBi) | -15 to -8 |
| Mismatch at band edges (dB) | -2 to 0 |
| Body / head shadowing (dB) | -25 to -5 |
| Phone antenna gain (dBi) | -6 to 0 |
| Fading margin taken (dB) | -6 to 0 |
| Phone sensitivity (dBm) | -95 to -90 |

| Distance | FSPL dB | margin P5 | margin P50 | P(link fails) |
|---:|---:|---:|---:|---:|
| 0.5 m | 34.2 | +17.9 | +28.9 | 0.0% |
| 1 m | 40.2 | +11.9 | +22.8 | 0.0% |
| 2 m | 46.2 | +5.7 | +16.8 | 0.1% |
| 3 m | 49.7 | +2.3 | +13.2 | 1.6% |
| 5 m | 54.2 | -2.2 | +8.8 | 10.6% |

## Reading it

- At 1 m (phone in hand or on a desk nearby) the link closes with margin in essentially all cases.
- At 3-5 m with the phone in a back pocket on the far side of the head, the worst corner fails. That's the case the 15 s log buffer exists for: data waits on the pod and syncs when the phone is back in range.
- The two terms that matter most are head shadowing and antenna efficiency. Measure both in EVT before trusting any range claim.