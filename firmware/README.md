# Firmware

Firmware for the Rev B.2 pod: an nRF52832 (WLCSP, nRF52832-CIAA) on Zephyr v4.4.2 with MCUboot, and a MAXM86161 optical module. Rev B.1 is archived (git tag `rev-b1`).

It builds for the custom board `atrium_pod`, and the host unit tests pass on `native_sim`. It has not run on hardware yet: see [What needs hardware](#what-needs-hardware).

| Folder | Contents |
|---|---|
| `boards/localforge/atrium_pod/` | Board: pin map, clocks, DC/DC mode, flash map, cell and charger constants |
| `drivers/` | MAXM86161 driver and binding (no in-tree driver for this part) |
| `src/power/` | Cut-off, charge policy, dock detection, battery estimate, wear detection, LED headroom, modes, NTC, power state machine. Plain C |
| `src/hw/` | Hardware glue: ADC (VBAT, VBUS, two NTCs), GPIO, status LED, IMU and its MLC program, and the PPG interface (`ppg.h`) with its MAXM86161 backend |
| `src/ble/` | GATT services of [`docs/ble_gatt.md`](../docs/ble_gatt.md) |
| `src/` | Main loop, pulse and HRV estimates, MLC activity class, oxygen trend, activity and posture, flash log, settings, DFU health. The Zephyr app build files are here too: `CMakeLists.txt`, `Kconfig`, `prj.conf`, `sysbuild.conf`, `sample.yaml`, `west.yml` |
| `tests/` | ztest suites on `native_sim` (`power/`, `app/`, with the ML reference vectors in `app/vectors/`), the build image (`Dockerfile`) and the helper scripts below |

## Features

Mapped to [`docs/FEATURES.md`](../docs/FEATURES.md). EVT features must work end to end on the first build.

| # | Feature | Status | Firmware | GATT (`docs/ble_gatt.md`) |
|---|---|---|---|---|
| 1 | Heart rate | EVT | Spectral track with accelerometer motion subtraction, 1 Hz, from green PPG (IR when green has no headroom) (`src/pulse.c`, port of `atrium_ml/hr.py`); logged per record | Heart Rate 0x180D, Pulse Detail 0x0203, log |
| 2 | HRV | V1 | Max-slope beats, 15 %-of-local-median cleaning, RMSSD per log record (`src/hrv.c`, port of `atrium_ml/hrv.py`) | Log record bytes 16–18 |
| 3 | Skin temperature | EVT | Skin NTC TH1 and top NTC TH4, logged | Skin Temperature 0x0301, log |
| 4 | Sleep | V1 | Inputs only: pulse, HRV, activity and posture in the log. Staging is app-side | Log |
| 5 | Sleep position | V1 | Head pitch and roll from the mean gravity vector, per log record (`src/activity.c`) | Log record bytes 13, 19 |
| 6 | Activity and effort | V1 | Activity index per minute (`src/activity.c`). LSM6DSOX machine-learning core motion level 0–7 every 3.12 s, and per record still, walk, run or other with steps/min (`src/mlc.c`, port of `atrium_ml/mlc.py`) | Activity 0x0401, log bytes 20–21 |
| 8 | Oxygen trend | Later | 30 s red/IR bursts: sleep every 5 min, smart every 30 min, only after 60 s still with a pulse. Relative red/IR ratio R with a confidence; no percentage on the pod (`src/spo2.c`, port of `atrium_ml/spo2.py`). Research only | Oxygen Ratio 0x0205, log bytes 22–23 |
| 7, 13 | Recovery, daily summary | V1 | None: app-side from the log | — |
| 15 | Pairing | EVT | LE Secure Connections, bonding, DIS name and firmware version | 1, 8 |
| 16 | Battery and charging | EVT | Battery estimate from VBAT_SENSE (`src/power/soc_est.c`): a coulomb count from the mode budgets, pulled towards the open-circuit voltage at light load; CHG#, VBUS_SENSE, charge policy | Battery 0x180F, Charge State 0x0101 |
| 17 | Case status | EVT | Dock state and charge state. The estimate and "charge your case" are app-side | Charge State 0x0101 |
| 18 | Wear detection | EVT | Learned on-skin DC, pulse presence from the unweighted spectrum, motion and the TH1 skin NTC (`src/power/wear.c`, port of `atrium_ml/wear.py`). Off-body pauses measurement to a 34 µA probe | Charge State: pod state 3, flag bit 3 |
| 19 | Ship mode | EVT | Cut-off and the ship command drive SHIP; a dock wakes the pod | Charge State, Command 0x02 |
| 20 | Log sync | EVT | 24-byte records, 1600 KB: 68 000 records, 11.8 days at 15 s (the phone syncs daily); resumable by sequence number | Log 0x0500 |
| 21 | Firmware update | EVT | The only field path on B.2. MCUboot swap-move, ECDSA P-256 signed, health-gated confirm, auto-revert of a bad image (`src/dfu_health.c`) | 9 |
| 23 | Privacy | EVT | Works without an account; encrypted bonded link; private addresses; factory reset | 10, Command 0x05 |

Features 9 to 12, 14 and 22 are Later or app and cloud items. The firmware has none of them. For #9 the two NTCs (skin TH1, top TH4) are read and logged; there is no heater.

## Toolchain

Everything runs in one Docker image, so a Mac or Linux host needs only Docker. `native_sim` does not run on macOS.

| Part | Version |
|---|---|
| Zephyr | v4.4.2. `src/west.yml` fetches only cmsis, cmsis_6, hal_nordic, hal_st, mbedtls, tf-psa-crypto, mcuboot, segger and zcbor |
| Zephyr SDK | 1.0.1, ARM toolchain only |
| Image | Ubuntu 24.04, x86_64 or aarch64, about 1.2 GB of content |

**Helper scripts** (the only two):

| Script | Usage | Does |
|---|---|---|
| `tests/fw.sh` | `firmware/tests/fw.sh image` | Builds the Docker image once (about 10 minutes) |
| | `firmware/tests/fw.sh test` | Runs every host test on `native_sim`, plus a build for `atrium_pod` |
| | `firmware/tests/fw.sh build` | Writes `build/atrium_pod_merged.hex` (MCUboot + signed app) and `build/src/zephyr/zephyr.signed.bin` (the update image) |
| | `firmware/tests/fw.sh shell` | Opens a shell in the image |
| `tests/check_pins.py` | `python3 -I firmware/tests/check_pins.py [--netlist PATH]` | Checks the board files against a pod netlist; exits 1 on a mismatch |

`build/` is git-ignored.

**Reference vectors.** The CSVs in `tests/app/vectors/` are Atrium synthetic signals. Regenerate them with `python3 app/python/make_test_vectors.py firmware/tests/app/vectors` (from PR #10).

Flashing runs on the host, outside Docker. Neither `nrfutil` nor a J-Link package was installed on the development machine when this was written.

The code uses only upstream Zephyr APIs. It should also build under nRF Connect SDK, but that is not checked [VERIFY].

## Board

| Net | U1 ball | nRF pin | Use |
|---|---|---|---|
| SDA / SCL | F7 / F6 | P0.06 / P0.07 | I²C 400 kHz: MAXM86161 0x62, LSM6DSOX 0x6A |
| FL_CS / FL_SCK / FL_MOSI / FL_MISO | H6 / G4 / H5 / H4 | P0.12 / 13 / 14 / 15 | SPIM1, 8 MHz, MX25R1635F |
| PPG_INT | B3 | P0.25 | MAXM86161 INTB, open drain; internal pull-up |
| PPG_EN | H3 | P0.16 | MAXM86161 LDO_EN: low when no optics run (0.05 µA) |
| PPG_SYNC | H2 | P0.18 | MAXM86161 GPIO, open drain; not used yet |
| IMU_INT1 / IMU_INT2 | A3 / F4 | P0.28 / P0.11 | LSM6DSOX data ready on INT1. The MLC also pulses INT2, but firmware polls MLC0_SRC every 39 samples instead, so INT2 is not configured |
| CHRG_N | B5 | P0.31 | BQ25101 CHG#, internal pull-up |
| CHG_INH | G7 | P0.08 | Low disables the BQ25101 through R16 to TS; Hi-Z otherwise |
| VBUS_SENSE | C6 | P0.02 / AIN0 | 1 MΩ / 470 kΩ, 1.57 V max; 0.6 V ref at gain 1/3, 40 µs acquisition |
| VBAT_SENSE | A4 | P0.29 / AIN5 | VBAT / 2 (R17 / R18 1 MΩ, C24 100 nF); 0.6 V ref at gain 1/4, 40 µs |
| NT1_SENSE / NT3_SENSE | D6 / A5 | P0.03 / AIN1, P0.30 / AIN6 | TH1 skin under the window, TH4 top under the lid. NCU15 10 kΩ, B 3380 K, over 10 kΩ 0.1 %; ratiometric against VDD/4 |
| NTC_PWR | F3 | P0.19 | High only during the NTC conversions |
| — | E6 / E7 | P0.04 / P0.05 | Not connected (no heater, no TH3) |
| SHIP | G2 | P0.20 | LPB1010 CTL: high for ≥ 100 ms ships the pod |
| LED_K | G3 | P0.17 | Status LED D2 (white, under the light pipe): VDD → R22 470 Ω → D2 → P0.17, about 1 mA when low |
| — | D7 / C7 | P0.00 / P0.01 | Not connected: no 32.768 kHz crystal |
| SWDIO / SWCLK / NRESET | G1 / F1 / H1 | —, —, P0.21 | Test pads TP5 / TP6 / TP7, no series resistors. Pin reset left off |

- **Pin check:** `tests/check_pins.py --netlist atrium-rev-b1/phase1/pod_pin_netlist_r1.csv` (the r5 Pill netlist, 79 mm²) reports 19 of 19 pins matching, and every unused U1 net explained.
  - The default path, `hardware/pod/schematic/pod_pin_netlist.csv`, still holds the archived B.1 netlist until the electrical PR updates it.
- **Rails and clocks:** VDD is 3.3 V from the TPS62840 (R2 267 kΩ). The nRF52832 regulator runs in DC/DC mode (DCC → L2 10 µH → L3 15 nH → DEC4). The clocks are 32 MHz Y1 and the internal LFRC, calibrated against the HFXO by the clock driver; the BLE sleep clock is 500 ppm. LFRC widens the BLE receive windows: about +4 µA while connected [ASSUMED]. VLED = VBAT directly (no ferrite).

**Cell and charger** are `zephyr,user` properties in `atrium_pod.dts`. Nothing in `src/` hard-codes them.

| Property | Value | Source |
|---|---|---|
| `cell-capacity-mah` | 30 | Sharvi 400909, 30 mAh, 9 × 9 × 4.0 mm, no protection board (the LPB1010 protects it); owner, 2026-11-07 [VERIFY data sheet] |
| `charge-current-max-ua` | 10902 | BQ25101 K_ISET max 145 AΩ / R1 13.3 kΩ (10.15 mA typical at 135 AΩ): 0.36C maximum |
| `cutoff-mv` | 3300 | Loaded VBAT; above LPB1010 VDL max 2.9 V |
| `green-min-mv` | 3500 | MAXM86161 green on the 31 mA range [VERIFY Vf against current] |
| `cell-resistance-mohm` | 2000 | [ASSUMED] end of life and cold (1.0 Ω typical); no cell data sheet yet. Set it from the bench |
| `vbat-sense-top-ohm`, `vbat-sense-bottom-ohm` | 1 MΩ, 1 MΩ | R17, R18 |

| Flash | Partition | Size |
|---|---|---|
| Internal | MCUboot | 64 KB (34.6 KB used) |
| Internal | Slot 0, app | 424 KB (257.0 KB used) |
| Internal | Settings and bonds (NVS) | 24 KB |
| MX25R1635F | Slot 1, update image | 424 KB at 0x000000 |
| MX25R1635F | Log | 1600 KB at 0x070000 |

RAM use is 58.7 KB of 64 KB. The spectral HR takes about 7 KB; the MLC class and the oxygen trend about 1.3 KB together.

## Programming and debug, from the schematic

**Pads.**
- **Pogo:** only VBUS (J1.1) and GND (J1.2), for the case.
- **SWD:** on internal test pads, reachable only on the bare-board jig, with no series resistors.
- **Field updates:** BLE only (section 9 of `docs/ble_gatt.md`).

| Pad | Net |
|---|---|
| TP1 | VBAT |
| TP2 | VDD 3.3 V (probe VTref) |
| TP3 | BATN |
| TP4 | SHIP |
| TP5 | SWDIO (U1 G1) |
| TP6 | SWCLK (U1 F1) |
| TP7 | NRESET (U1 H1) |

Positions follow the Rev B.2 floorplan, which is still in progress [VERIFY against the released Gerbers].

**Power on the jig.**
- A bare PCBA has no cell. Feed 3.8 V, current-limited to 50 mA, into TP1 (VBAT), with TP3 (BATN) as the return.
- With a cell fitted, 5 V on J1.1 holds VBAT up and wakes the pod from ship mode.

**Probe.** Use any SWD probe at 3.3 V I/O, such as a J-Link or a CMSIS-DAP probe, with VTref from TP2.

**Pin reset stays off.**
- UICR.PSELRESET is left erased, so P0.21 is a plain GPIO. SWD resets the core without the pin.
- For bench bring-up only, an overlay with `&uicr { gpio-as-nreset; };` turns the pin on.

**First flash.**

```bash
nrfutil device recover
```

```bash
nrfutil device program --firmware build/atrium_pod_merged.hex --options chip_erase_mode=ERASE_ALL
```

- `recover` erases the chip through CTRL-AP. It works whatever firmware is on it.
- `west flash` from the build directory works too: the board defines the nrfutil, nrfjprog, J-Link, pyOCD and OpenOCD runners.
- [VERIFY: run `nrfutil device --help` and check the option names before scripting a jig.]
- APPROTECT: the build uses `NRF_APPROTECT_USE_UICR`. Debug stays open as long as UICR.APPROTECT reads HwDisabled after `recover` [VERIFY on the first silicon revision received]. Whether to lock production units is an owner decision.

**Logs.** SEGGER RTT over SWD, for example with `JLinkRTTClient`. The pod has no UART.

**Updates after that.** MCUmgr SMP over BLE, as in `docs/ble_gatt.md` section 9.
- Upload `build/src/zephyr/zephyr.signed.bin`.
- There is no upload below 3.60 V off the dock.
- A new image confirms itself once it is healthy after 60 s. If it is still unhealthy at 300 s, it reboots, and MCUboot restores the previous image.
- The image is signed with MCUboot's development ECDSA P-256 key. Replace the key before any release [VERIFY].

## Power and charge logic

All of it is plain C in `src/power/`, stepped once a second by `src/main.c`. The rules are `atrium-rev-b1/phase1/sim/SCHEMATIC_SIM.md` §5.4 with the Rev B.2 constants above, kept consistent with `logic_sim.py`.

| Rule | Implementation | Test |
|---|---|---|
| Cut-off at 3.30 V loaded, then ship | Three VBAT_SENSE readings in a row below 3.30 V (read every 10 s), off the dock only. Then SHIP is held high for 500 ms: tSM is 30 to 70 ms. Released only by a dock | `test_cutoff.c`, including a discharge to the trip point |
| Low battery | Below cut-off + 150 mV, the mode drops to lite. It clears above cut-off + 250 mV [ASSUMED margins] | `test_cutoff.c`, `test_power_sm.c` |
| No charge below 0 °C or above 45 °C | CHG_INH low. TH4 (top) alone is the cell proxy: the r5 board has no TH3. TH1 sits on the skin, so it is left out. The cell's own NTC TH2 stops the charger in hardware. Leaving a band needs 1 °C of hysteresis | `test_charge_policy.c`: sweep from −10 to 60 °C |
| 0 to 10 °C, ≤ 0.2C | 60 s duty cycle against the worst-case half current, minus one update period. At 30 mAh the 6 mA cap is above the 5.45 mA half current, so the charger runs unbroken; the duty cycle covers smaller cells (20 mAh: 73 %) | `test_b2_constants_keep_the_invariant`, `test_cold_average_at_most_0p2c` |
| Fail-safe | No fresh TH4 reading (over 10 s old, or open) means CHG_INH low. The policy still supports a backup sensor (disagreement over 10 °C is a fault); r5 passes none | `test_charge_policy.c`, `test_primary_alone` |
| Dock detection | VBUS above 4.0 V twice to dock, below 3.5 V twice to undock. A lift is seen in under 1 s | `test_dock.c` |
| Charge state | CHG# only marks the first cycle, so VBAT under the charger's 4.10 V recharge level also counts as charging | `test_dock.c` |
| Battery % (no gauge) | Coulomb count from the mode budgets, and ICHG while charging. Pulled towards the open-circuit voltage (VBAT + I × R) at light load. Snaps to 100 % when CHG# releases | `test_soc_est.c` |
| LED headroom | The LEDs run from VBAT. Below green-min-mv plus the pulse sag (15 mA × 2 Ω = 30 mV), green swaps to IR at the same current. 50 mV hysteresis | `test_headroom.c` |
| Wear detection (#18) | Port of `atrium_ml/wear.py`. Off-body when any of these holds:<br>- dark (DC below 0.15 of the learned on-skin level) for 4 s;<br>- no pulse for 60 s while still and cool (TH1 below 30 °C or falling 0.1 °C/min);<br>- no pulse for 300 s below 32 °C.<br>Worn again when lit for 2 s and then a pulse within 15 s. Pulse presence comes from the unweighted spectrum, never the HR confidence. Off-body runs one 5 mA green slot at 8 Hz. Thresholds are [ASSUMED] | `test_wear.c`, `test_ml_vectors.c` |
| Docked idle ≤ 0.07 mA | Optics unpowered (LDO_EN low), IMU off, NTCs at 0.1 Hz, advertising every 2 s: 17 µA typical, 25 µA worst case | `test_modes.c` |
| Modes | The ML data contract: green at 25 Hz, accelerometer at 12.5 Hz, lite optics 30 s in every 120 s. Each LED at most once (MAXM86161) | `test_modes.c`, `test_maxm86161.c` |
| Motion level and activity class | Ports of `atrium_ml/mlc.py` (PR #11). The LSM6DSOX program (97 register writes, ST's BSD-3 motion_intensity tree with Atrium's 7 thresholds) goes in after the driver's init and leaves the accelerometer at 12.5 Hz, ±2 g. Per record: still when every level is 0; walk at median level 4–5 with a step rhythm of 70–150 steps/min; run at level ≥ 6 and 140–200 steps/min; else other. Thresholds [ASSUMED] | `test_mlc.c`: the program byte for byte, the thresholds, the ladder and the class on 4 cases. A periodic Hann window fails it |
| Oxygen trend bursts | Port of `atrium_ml/spo2.py` (PR #11). IR 10 mA and red 10 mA join the mode's green slot for 30 s; green stays slot 0, so pulse and HRV run on. 12 windows of 4 s; gates: correlation ≥ 0.8, IR perfusion ≥ 0.02 %, motion ≤ 15 mg, pulse present, 5–95 % of the ADC range [ASSUMED] | `test_spo2.c`: every window and the burst on 4 cases, R within 0.5 %; the schedule. Unprimed filters fail it |
| Pulse and HRV algorithms | Ports of `app/python/atrium_ml` (PR #10).<br>- HR: spectral track, 8 s, 256-point FFT, accelerometer subtraction, tracking.<br>- HRV: max-slope fiducial, intervals clean within 15 % of the local median; RMSSD needs at least 5 clean differences. | `test_ml_vectors.c` against the reference on 5 synthetic cases: bpm within 0.5, beat times within 2 ms, RMSSD within 1 ms, flags equal. Restoring the old 20 % rule fails it |

**Modes and budgets** (µA at the cell, [ASSUMED] until measured; ML data contract). Not in the table:
- **MLC:** 1–3 µA [VERIFY] while the accelerometer runs.
- **Oxygen bursts:** +14 µA in sleep, +2.4 µA in smart (PR #11 contract).
- **LFRC:** about +4 µA while connected [ASSUMED].


| Mode | Slots | Rate | Optics duty | Typical | Worst case |
|---|---|---|---|---|---|
| full | green 15, IR 10, red 10 mA | 25 Hz | always | 437 | 517 |
| smart (default) | green 15 mA | 25 Hz | always | 215 | 247 |
| lite | green 10 mA | 25 Hz | 30 s in 120 s | 70 | 80 |
| sleep | green 10 mA | 25 Hz | always | 129 | 154 |
| docked idle | none | — | — | 17 | 25 |
| off-body | green 5 mA | 8 Hz | always | 34 | 48 |

How the budgets are built:
- **Rest of the pod:** the s4 model's non-optical currents, conservative for the 3.3 V DC/DC rail [VERIFY].
- **MAXM86161 module:** 1.33 µA typical and 2.0 µA maximum per exposure per sps.
- **LEDs:** I × 123.8 µs × 1.21 per pulse.
- **Battery life:** 16 h smart and 8 h sleep, with all of the above, average 199 µA typical and 229 µA worst case: 6.3 days typical and 5.5 worst case on the full 30 mAh; 4.4 days worst case if only 80 % is usable to the 3.30 V cut-off on an aged or cold cell [ASSUMED].

**Bug the tests caught.** With 1 s updates, the cold-band on phase rounded up to 40 s of every 60 s, which went over the 0.2C limit. The policy now ends the on phase one update early, and the test fails without that fix.

## Drivers

| Part | Driver |
|---|---|
| MAXM86161 | `drivers/maxm86161/`. LDO_EN power gating, part ID 0x36 checked at each power-up, up to 3 slots at one rate, FIFO drained on A_FULL, low-power mode between samples |
| LSM6DSOX | Zephyr `st,lsm6dso` (same register set), data-ready trigger on INT1 |
| MX25R1635F | Zephyr `jedec,spi-nor` (C2 28 15), deep power-down, 4 KB layout pages |
| LSM6DSOX MLC | `src/mlc.c` program, written over I²C after the driver's init; MLC0_SRC read on the embedded page, under a bus lock against the driver's ODR calls |
| Status LED | `gpio-leds` on P0.17, `src/hw/status_led.c`: 3 flashes at boot, 1 every 4 s while charging, 2 when the temperature blocks charging |
| NTCs, VBAT, VBUS | SAADC, `src/hw/power_hw.c` |

## What needs hardware

1. **Bring-up and current.**
   - Rails, then SWD at 3.3 V through TP5 to TP7 with VTref on TP2, then RTT logs.
   - DC/DC mode: check the L2/L3 order against PS v1.9 Figure 5 before enabling it.
   - Docked-idle current: target ≤ 70 µA, modelled at 17 µA.
   - Every mode against the budget table.
2. **Ship mode.**
   - SHIP high actually ships, and the pod draws ≤ 0.1 µA from the cell.
   - A dock wakes it.
   - The pod survives the 500 ms hold if VBUS appears.
3. **CHG_INH.**
   - Driving it low through R16 1 kΩ really stops the BQ25101 (TS disable threshold against 50 µA × 1 kΩ = 50 mV).
   - The charge restarts cleanly each cold-band period.
   - Whether CHG# reasserts after an inhibit.
4. **Temperature proxies.**
   - TH4 against the cell NTC TH2, docked in a warm and a cold case: TH4 is now the only firmware proxy.
   - VBAT_SENSE against a meter, and the battery estimate against a discharge curve.
   - The NCU15 beta model against the maker's R-T table, and the NTC_PWR pin drop (about 1 %).
5. **VBUS_SENSE.**
   - The thresholds at the real dock voltage.
   - The fall time after a lift.
   - Injection into AIN0 while the pod is unpowered.
6. **MAXM86161.**
   - The time from LDO_EN high to the first I²C access (2 ms assumed).
   - Multi-item FIFO burst reads.
   - Green Vf and the real green-min-mv.
   - Counts per mA on skin.
7. **Cell.** Cell resistance against temperature and age, to set `cell-resistance-mohm` and confirm the cut-off margin.
8. **Wear detection.** Set the green DC thresholds on skin, in air, on a table and in the case.
9. **Pulse, HRV and posture.**
   - Pulse against a chest-strap reference, at rest and moving.
   - RMSSD against an ECG at night.
   - The PPG polarity: beats are taken at local maxima.
   - The IMU axes for pitch and roll on the B.2 placement.
   - The default on-skin DC reference (10 000 counts/mA) before the first learned minute.
   - These are wellness estimates only.
10. **MCUboot.**
    - A swap-move update with the secondary slot on the MX25R1635F, end to end over BLE.
    - Revert of an unhealthy image.
11. **Radio.** Advertising and connection current, range on the head, MTU 247 and DLE with the app's phones.
12. **LFRC.** The calibrated LFRC holds 500 ppm across temperature, BLE stays connected through a night, and the connected current with LFRC.
13. **MX25R1635F.** JEDEC ID C2 28 15 read at boot, 8 MHz in its default ultra-low-power mode, and deep power-down entry and exit (tDP 10 µs, tRDP 35 µs assumed) [VERIFY against its data sheet].
14. **MLC.**
    - The INT2 routing addresses (MD2_CFG 0x5F, MLC_INT2 0x11) and MLC0_SRC (0x70) [VERIFY].
    - Bench check: still on a table level 0, shaken 5–7, walking on the head 3–5.
    - Its current on top of the accelerometer.
15. **Oxygen trend.** R against a reference oximeter overnight (VALIDATION.md V5); the burst current; the HR track through the AFE restart at burst start and end. No calibration and no percentage until V5.
16. **Status LED.** Brightness through the light pipe at 1 mA, and a 20 ms flash visible in daylight. Taps (from IMU INT1) are not implemented: LSM6DSOX tap detection needs a far higher ODR than the 12.5 Hz the modes run [VERIFY current].
