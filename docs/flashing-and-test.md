# Flashing and test

How to reach the pod's SWD port, bring up a new pod and test it in production.

## Flashing pins

Four pads on the base reach the nRF52832 SWD port: **VIN, GND, SWDIO and SWDCLK** (coordinates in [architecture.md](architecture.md#contacts-base-skin-side)). They are ENIG pads on the flex bottom, recessed in the base holes (base 0.35 mm plus 0.03 mm bond).

- **Logic level is 1.8 V.** Use a probe that follows target voltage: a SEGGER J-Link (VTref), or an nRF52 / nRF53 DK through its Debug-out header (it senses VTG). Don't use a fixed 3.3 V CMSIS-DAP probe without a level shifter.
- The pod has no VTref pad. The jig powers the pod from **VIN = 5 V** (the BQ21061 brings up VSYS and the 1.8 V rail) and feeds **1.8 V** from its own regulator into the probe's VTref.
- SWD is reachable whenever VIN is present, even with a flat or missing cell: the BQ21061 runs the system from VIN through PMID.

## Programming jig

CAD in `mechanical/programming-jig/` (STEP, STL, drawings).

- The nest is the pod outline + 0.10 mm, 2.0 mm deep. Put the pod in **window end toward the arrow**.
- Four spring probes, P75 class (Ø1.02 mm plunger) in Ø1.7 mm receptacles [VERIFY with the probe you buy]. Bed-of-nails style: the probes come up from below, and a lid or a finger holds the pod down.
- Wiring: VIN to 5 V through a 100 mA current limit; GND; SWDIO / SWDCLK to the probe; the jig's 1.8 V LDO to the probe's VTref.

## First power-up (EVT bring-up)

1. Before power, measure resistance from VIN, VSYS and VDD_1V8 to GND. All should read > 1 kΩ.
2. Put the pod in the jig on a current-limited 5 V supply (50 mA limit). VIN current should settle under 20 mA with the cell disconnected.
3. Measure VSYS (4.5 V) and VDD_1V8 (1.80 V) at their test points (listed in the PCB rules in `hardware/design-data/`).
4. Run `nrfjprog --recover` or `west flash --runner jlink`, then flash the bring-up build of the pod firmware in `firmware/`.
5. Watch RTT: all three I2C devices answer (the BQ21061 only while VIN is present), TH1 and TH4 read near room temperature, and VSYS is sensible.
6. Connect the cell. Check that CHG_PG goes low on VIN, that the charge current is about 15 mA once the firmware sets ICHG, and that charging terminates.
7. RF: a conducted test through a pigtail at the pi-match pads, then OTA RSSI with a phone (the pod advertises as "Atrium").

## Field updates

MCUboot with two 220 KB slots, updated over BLE (SMP / mcumgr). The SWD pads are the recovery path if a bootloader is ever bricked. The charging case can also reset the pod over SWD (see [charging-case.md](charging-case.md)).

## Production test (per pod)

| Step | Check | Limit |
|---|---|---|
| 1 | VIN current, cell disconnected | < 20 mA |
| 2 | Flash and verify | Pass |
| 3 | I2C IDs (IMU 0x6C WHO_AM_I, MAXM86161 PART_ID, BQ21061 STAT0 readable) | Pass |
| 4 | TH1, TH4 vs ambient | ±1.0 °C |
| 5 | Charge current at the 15 mA setting | 13–17 mA |
| 6 | RSSI at 1 m vs a golden pod | Within 4 dB |
| 7 | Ship mode, then wake on VIN | Pass |

The full end-of-line sequence, with the PPG and seal checks, is in [build-assembly-test.md](build-assembly-test.md#10-end-of-line-tests).
