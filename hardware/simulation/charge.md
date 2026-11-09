# Charging (BQ21061, 15 mA, CC/CV, generic cell model)

- Time to 80 %: 96 min. Time to termination (1.5 mA): 126 min. Final SOC 100%.
- Charger dissipation at 5 V in: ~18 mW. The pod barely warms; TH4/TS still gate charging at 0-45 degC.
- Default ICHG out of reset is not given in the BQ21061 excerpt I checked [VERIFY]. Firmware writes 15 mA on every VIN-good event; the TS NTC (TH5) and the charger's own thermal regulation are the hardware backstops if firmware isn't running.
- Cell model is generic. Replace OCV/R_INT with a measured curve from two real 400909 cells.