# Flashing and test

How to reach the pod's SWD port, bring up a new pod and test it in production.

## Flashing pins

Five ENIG pads on the flex bottom show through the base: **VIN, GND_A, GND_B, SWDIO and SWDCLK** (coordinates in [architecture.md](architecture.md#contacts-base-skin-side)). Programming uses four of them: VIN, one GND, SWDIO and SWDCLK. They are recessed in the base holes (base 0.35 mm plus 0.03 mm bond).

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
4. Run `nrfutil device recover`, then flash the bring-up build of the pod firmware (`firmware/tests/fw.sh debug`, RTT logs, no MCUboot).
5. Watch RTT: all three I2C devices answer (the BQ21061 only while VIN is present), TH1 and TH4 read near room temperature, and VSYS is sensible.
6. Connect the cell. Check that CHG_PG goes low on VIN, that the charge current is about 15 mA once the firmware sets ICHG, and that charging terminates.
7. RF: a conducted test through a pigtail at the pi-match pads, then OTA RSSI with a phone (the pod advertises as "Atrium").

## Programming the nRF52832

Build with `firmware/tests/fw.sh build` (Docker, Zephyr v4.4.2). It writes `firmware/build/atrium_pod_merged.hex` (MCUboot and the signed app) for SWD and `firmware/build/src/zephyr/zephyr.signed.bin` for BLE updates. It then checks that the signed image's header version and the Firmware Revision string inside it both match `firmware/src/VERSION`.

```bash
nrfutil device recover
```

```bash
nrfutil device program --firmware firmware/build/atrium_pod_merged.hex --options chip_erase_mode=ERASE_ALL
```

```bash
nrfutil device reset
```

With nrfjprog the same steps are `nrfjprog --recover`, `nrfjprog --program firmware/build/atrium_pod_merged.hex --chiperase --verify` and `nrfjprog --reset`. `west flash` from the build directory also works. [VERIFY the option names with `nrfutil device --help` before scripting a jig.]

- **Flash map** (512 KB): MCUboot 0x00000 (32 KB), slot 0 0x08000 (220 KB), slot 1 0x3F000 (220 KB), settings and bonds 0x76000 (8 KB), log 0x78000 (32 KB).
- **Debug access.** The build uses `CONFIG_NRF_APPROTECT_USE_UICR` and the hex holds no UICR records, so a recovered chip stays open to the debugger after programming. The production lock (UICR.APPROTECT written at the end of the factory flash) is a DVT decision; `recover` always works, and wipes the chip.
- **Signing key.** Production builds must be signed with your own ECDSA P-256 key: `ATRIUM_SIGNING_KEY=/path/to/atrium-ecdsa-p256.pem firmware/tests/fw.sh build`. Without it the build uses MCUboot's public development key, which is for bench units only. A pod accepts BLE updates signed with the key it was flashed with. Never commit a key.
- **Version.** The image is 0.1.1 (`firmware/src/VERSION`); the DIS Firmware Revision reads `0.1.1+<commit>`.

## Programming the charging case

The case MCU is a CH32V003 on a single-wire debug interface (SWIO), programmed with a WCH-LinkE and `minichlink` through TP1 (SWIO, PD1), TP2 (3.3 V) and TP3 (GND) on the case board, before the case is closed, with no cell fitted when TP2 is used: `minichlink -w firmware/case/build/atrium_case.bin flash -b`, then `minichlink -D` once so PD7 is a GPIO. Build steps are in [`firmware/case/README.md`](../firmware/case/README.md). The case has no field update path.

## Field updates

MCUboot with two 220 KB slots, updated over BLE (SMP / mcumgr). The SWD pads are the recovery path if a bootloader is ever bricked. The charging case can also reset the pod over SWD (see [charging-case.md](charging-case.md)).

## Production test (per pod)

| Step | Check | Limit |
|---|---|---|
| 1 | VIN current, cell disconnected | < 20 mA |
| 2 | Flash and verify | Pass |
| 3 | I2C IDs (IMU 0x6C WHO_AM_I, MAXM86161A PART_ID 0x36, BQ21061 STAT0 readable) | Pass |
| 4 | TH1, TH4 vs ambient | ±1.0 °C |
| 5 | Charge current at the 15 mA setting | 13–17 mA |
| 6 | RSSI at 1 m vs a golden pod | Within 4 dB |
| 7 | Ship mode, then wake on VIN | Pass |

The full end-of-line sequence, with the PPG and seal checks, is in [build-assembly-test.md](build-assembly-test.md#10-end-of-line-tests).
