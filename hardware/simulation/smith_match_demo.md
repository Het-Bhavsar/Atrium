# Pi-match search: synthetic detuned antenna (demo)

Unmatched worst |S11| in band: -4.4 dB

## Best 2-element

- -14.3 dB: sh C 2.0pF -> se L 1.0nH
- -14.2 dB: sh C 1.8pF -> se L 0.6nH
- -14.1 dB: sh C 2.2pF -> se L 1.2nH
- -14.0 dB: sh C 1.8pF -> se L 0.8nH
- -14.0 dB: sh C 2.0pF -> se L 0.8nH

## Best 3-element (shunt-series-shunt)

- -16.2 dB: sh C 3.3pF -> se L 3.3nH -> sh C 1.5pF
- -15.6 dB: sh C 3.9pF -> se L 3.0nH -> sh C 2.0pF
- -15.4 dB: sh C 2.7pF -> se L 3.3nH -> sh C 1.0pF
- -15.2 dB: sh C 4.7pF -> se L 2.4nH -> sh C 2.7pF
- -15.0 dB: sh C 2.7pF -> se L 3.3nH -> sh C 0.9pF

Elements listed from the antenna side towards the nRF. Target <= -10 dB across 2402-2480 MHz, <= -15 dB is good.
Re-measure with the zirconia shell on and the pod on a head (or phantom); fit, then re-measure. Two or three iterations is normal.