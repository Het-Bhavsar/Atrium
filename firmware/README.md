# Firmware

Firmware for the EVT pod (baseline 20.9 × 11 × 5.3 mm, approved 2026-11-07): an nRF52832 (WLCSP, nRF52832-CIAA) on Zephyr v4.4.2 with MCUboot, a MAXM86161 optical module, an LSM6DSOX and a BQ21061 charger. Rev B.1 is archived (git tag `rev-b1`).

It builds for the custom board `atrium_pod`, and the host unit tests pass on `native_sim`. It has not run on hardware yet: see [What needs hardware](#what-needs-hardware).

| Folder | Contents |
|---|---|
| `boards/atrium/atrium_pod/` | Board: pin map, clocks, DC/DC mode, flash map, cell and supply constants |
| `drivers/` | MAXM86161 driver and binding; BQ21061 register logic and binding (no in-tree drivers for these parts) |
| `src/power/` | Cut-off, charge policy, dock detection, battery estimate, wear detection, LED headroom, modes, NTC, power state machine. Plain C |
| `src/hw/` | Hardware glue: ADC (VSYS, two NTCs), System OFF, the charger on I²C and /PG, IMU and its MLC program, and the PPG interface (`ppg.h`) with its MAXM86161 backend |
| `src/ble/` | GATT services of [`docs/ble_gatt.md`](../docs/ble_gatt.md), and pairing on lift |
| `src/` | Main loop, pulse and HRV estimates, MLC activity class, oxygen trend, activity and posture, the 30 s epoch and its log, settings, DFU health. The Zephyr app build files are here too: `CMakeLists.txt`, `Kconfig`, `prj.conf`, `debug.conf`, `debug.overlay`, `sysbuild.conf`, `sysbuild/mcuboot.conf`, `sample.yaml`, `west.yml` |
| `case/` | The charging case MCU (CH32V003F4U6, C, no RTOS): its own project, [`case/README.md`](case/README.md) |
| `tests/` | ztest suites on `native_sim` (`power/`, `app/`, with the ML reference vectors in `app/vectors/`), the build image (`Dockerfile`) and the helper scripts below |

## Features

Mapped to [`docs/FEATURES.md`](../docs/FEATURES.md). EVT features must work end to end on the first build.

| # | Feature | Status | Firmware | GATT (`docs/ble_gatt.md`) |
|---|---|---|---|---|
| 1 | Heart rate | EVT | Spectral track with accelerometer motion subtraction, 1 Hz, from green PPG (IR when green has no headroom) (`src/pulse.c`, port of the ML reference model); logged per epoch | Heart Rate 0x180D, Pulse Detail 0x0203, log |
| 2 | HRV | V1 | Max-slope beats at 25 Hz with parabolic timing, 15 %-of-local-median cleaning, RMSSD per epoch (`src/hrv.c`, port of the ML reference model) | Log bytes 2–3 |
| 3 | Skin temperature | EVT | Skin NTC TH1 logged per epoch; TH1 and the top NTC TH4 live | Skin Temperature 0x0301, log byte 6 |
| 4 | Sleep | V1 | Inputs only: pulse, HRV, activity and posture in the log. Staging is app-side | Log |
| 5 | Sleep position | V1 | Head pitch and roll from the mean gravity vector, per epoch, in the pod frame (worn upright, window and antenna end up; #26). The board's `imu-to-pod` matrix turns the chip axes into it (`src/activity.c`) | Log bytes 8–9 |
| 6 | Activity and effort | V1 | Activity index per minute and per epoch (`src/activity.c`). LSM6DSOX machine-learning core motion level 0–7 every 3.12 s, and per epoch still, walk, run or other with steps (`src/mlc.c`, port of the ML reference model). Accelerometer only: the gyro stays off | Activity 0x0401, log bytes 3–5, 7 |
| 8 | Oxygen trend | Later | 30 s red/IR bursts at 25 Hz: sleep every 5 min, smart every 30 min, only after 60 s still with a pulse. Relative red/IR ratio R with a confidence; no percentage on the pod (`src/spo2.c`, port of the ML reference model). Research only | Oxygen Ratio 0x0205, log bytes 10–11 |
| 7, 13 | Recovery, daily summary | V1 | None: app-side from the log | — |
| 15 | Pairing | EVT | LE Secure Connections only, 16-byte keys, bonding; unencrypted links dropped after 30 s. New pairings only for 60 s after a lift from the Plinth (and after boot); bonded phones reconnect at any time | 1, 8 |
| 16 | Battery and charging | EVT | Battery estimate from VSYS off the Plinth (`src/power/soc_est.c`); BQ21061 at 15 mA, its TS limits and the firmware charge policy; charge done from the charger | Charge State 0x0101 (byte 5 SoC) |
| 17 | Plinth status | EVT | Plinth state from /PG (debounced) and charge state from the BQ21061. The skin pad covers the contacts, so /PG means the pad is off: no pad case in firmware | Charge State 0x0101 |
| 18 | Wear detection | EVT | One learned on-skin reference per LED (green, IR), pulse presence from the unweighted spectrum, motion and the TH1 skin NTC (`src/power/wear.c`, port of the ML reference model). Off-body pauses measurement to a probe mode | Charge State: pod state 3, flag bit 3 |
| 19 | Ship mode | EVT | On the Plinth the ship command arms the BQ21061 ship mode, taken at the next lift. Off the Plinth, the cut-off and the command power the pod down to System OFF; the Plinth wakes it | Charge State, Command 0x02 |
| 20 | Log sync | EVT | One 12-byte record per 30 s epoch in page-packed blocks, 8 pages (32 KB) of internal flash: 19.8–22.6 h | Log 0x0500; Live Epoch 0x0504 |
| 21 | Firmware update | EVT | The only field path. MCUboot swap-move, ECDSA P-256 signed, health-gated confirm, auto-revert of a bad image (`src/dfu_health.c`) | 9 |
| 23 | Privacy | EVT | Works without an account; encrypted bonded link; private addresses; factory reset | 10, Command 0x05 |
| 24 | Fit guide | EVT | Fit mode holds green at 25 Hz before wear says worn. Each second over the last 4 s of the pulse LED: regularity (autocorrelation at 40–180 bpm), pulsatile perfusion and q 0–100, with moving, saturated, dark and pulse-present flags, and "fit good" for the app's green (`src/sigq.c`, port of the ML reference model) | PPG Control bit 2, Fit Quality 0x0206 |

Features 9 to 12, 14, 22, 25 (the skin pad) and 26 (wear orientation, beyond the pod frame of #5) are Later or app items; the firmware has nothing more for them. For #9 the two NTCs are read; TH4 sits next to the nRF and reads its self-heating, so it is not logged.

## Toolchain

Everything runs in one Docker image, so a Mac or Linux host needs only Docker. `native_sim` does not run on macOS.

| Part | Version |
|---|---|
| Zephyr | v4.4.2. `src/west.yml` fetches only cmsis, cmsis_6, hal_nordic, hal_st, mbedtls, tf-psa-crypto, mcuboot, segger and zcbor |
| Zephyr SDK | 1.0.1, ARM toolchain only |
| Image | Ubuntu 24.04, x86_64 or aarch64, about 1.2 GB of content |

**Helper scripts** (the pod's two, then the case's):

| Script | Usage | Does |
|---|---|---|
| `tests/fw.sh` | `firmware/tests/fw.sh image` | Builds the Docker image once (about 10 minutes) |
| | `firmware/tests/fw.sh test` | Runs every host test on `native_sim`, plus a build for `atrium_pod` |
| | `firmware/tests/fw.sh build` | Writes `build/atrium_pod_merged.hex` (MCUboot + signed app) and `build/src/zephyr/zephyr.signed.bin` (the update image) |
| | `firmware/tests/fw.sh debug` | Writes `build/debug/zephyr/zephyr.hex`: the bring-up image with RTT logs and no MCUboot |
| | `firmware/tests/fw.sh bench` | Writes `build/vib_bench/zephyr/zephyr.hex`: the vibration bench rig for an nRF52 DK (`tests/vib_bench`, `docs/ble_gatt.md` 4.5), not a pod image |
| | `firmware/tests/fw.sh shell` | Opens a shell in the image |
| `tests/check_pins.py` | `python3 -I firmware/tests/check_pins.py [--netlist PATH]` | Checks the board files against the pod netlist (`hardware/design-data/netlist/atrium_nets.yaml` by default); exits 1 on a mismatch |
| `case/tools/case.sh` | `firmware/case/tools/case.sh image \| test \| build \| shell` | The case toolchain image, its host tests, and `case/build/atrium_case.hex` |
| `case/Makefile` | `make -C firmware/case test` | The case host tests with the local C compiler, no Docker |

`build/` is git-ignored.

**Reference vectors.** The CSVs in `tests/app/vectors/` are Atrium synthetic signals. They come from the ML reference models. The log vectors in `tests/app/src/test_log_record.c` and `test_logpack.c` come from the ML reference study: `encode()` per record, and `pack_pages()` over three runs (crc32 of the pages).

Flashing runs on the host, outside Docker. Neither `nrfutil` nor a J-Link package was installed on the development machine when this was written.

The code uses only upstream Zephyr APIs. It should also build under nRF Connect SDK, but that is not checked [VERIFY].

## Board

From the EVT baseline netlist (`hardware/design-data/netlist/atrium_nets.yaml`) and `docs/architecture.md`.

| Net | U1 ball | nRF pin | Use |
|---|---|---|---|
| I2C_SDA / I2C_SCL | H4 / H3 | P0.15 / P0.16 | I²C 400 kHz at 1.8 V, R1 / R2 4.7 kΩ: MAXM86161 0x62, LSM6DSOX 0x6A, BQ21061 0x6B (the charger only on the Plinth) |
| PPG_INT | H2 | P0.18 | MAXM86161 INTB, open drain; internal pull-up |
| PPG_LDO_EN | G2 | P0.20 | MAXM86161 LDO_EN: low when no optics run (0.05 µA); R5 1 MΩ pull-down |
| IMU_INT1 | H6 | P0.12 | LSM6DSOX data ready, push-pull, no pull (INT1 must be low or floating at IMU power-up). INT2 is not connected; firmware polls MLC0_SRC every 39 samples |
| CHG_PG | H5 | P0.14 | BQ21061 /PG, open drain, R3 100 kΩ pull-up. Low on the Plinth: dock state, and the System OFF wake source |
| NTC_EN | G3 | P0.17 | High only during the NTC conversions |
| TH1_SENSE | D6 | P0.03 / AIN1 | Skin NTC. NCP03XH103F05RL 0201 10 kΩ 1 %, B 3380 K, under R10 10 kΩ 0.1 %; ratiometric against VDD/4; temperature from Murata's R-T table (`src/power/ntc.c`) |
| TH4_SENSE | C6 | P0.02 / AIN0 | Top NTC on the radio island next to the nRF, same divider (R11). Moved from P0.30 / AIN6 on 2026-11-08 |
| VSYS_SENSE | A3 | P0.28 / AIN4 | VSYS / 3.2 (R40 2.2 MΩ / R41 1 MΩ, C12 100 nF); 0.6 V ref at gain 1/3, 40 µs |
| NRESET_TP | H1 | P0.21 | TP1: pin reset for the bench (`&uicr { gpio-as-nreset; }`) |
| SWDIO / SWDCLK | G1 / F1 | — | Flex contact pads through 47 Ω and ESD diodes |

- **Not connected:** P0.00, P0.01 (no 32.768 kHz crystal), P0.04 to P0.11, P0.13, P0.19, P0.22 to P0.27, P0.29 to P0.31. No LED, no buttons, no UART, no external flash.
- **Trace pins:** P0.14, P0.15, P0.16 and P0.18 (TRACEDATA[3:0], P0.18 also SWO) and P0.20 (TRACECLK) carry nets here [VERIFY: nRF52832 PS pin assignments]. They are plain GPIO until TRACECONFIG is written; nothing does that: no SWO log backend, no ETM, no TRACECONFIG write.
- **Pin check:** `tests/check_pins.py` against the baseline netlist: 10 of 10 pins match and P0.21 is explained.
- **Rails and clocks:** VDD 1.8 V from the TPS7A0218 (U2) on VSYS; the IMU has its own TPS7A0218 (U13, no enable pin). The nRF52832 regulator runs in DC/DC mode. The clocks are the 32 MHz crystal Y1 and the internal LFRC, calibrated against the HFXO by the clock driver; the BLE sleep clock is 500 ppm. LFRC widens the BLE receive windows: about +4 µA while connected [ASSUMED]. MAXM86161 VLED = VSYS.

**Cell and supply** are `zephyr,user` properties in `atrium_pod.dts`; the charge currents are on the `ti,bq21061` node. Nothing in `src/` hard-codes them.

| Property | Value | Source |
|---|---|---|
| `cell-capacity-mah` | 30 | Sharvi 400909, 30 mAh; AP6683 protection [VERIFY data sheet] |
| `constant-charge-current-max-microamp` | 15000 | 0.5C; ICHG code 0x0C (1.25 mA steps) |
| `cold-charge-current-microamp` | 5000 | Under 0.2C (6 mA), set while TH4 reads 0 to 10 °C |
| `cutoff-mv` | 3300 | Loaded VSYS off the Plinth |
| `green-min-mv` | 3500 | MAXM86161 green on the 31 mA range, on VLED = VSYS [VERIFY Vf against current] |
| `cell-resistance-mohm` | 2000 | [ASSUMED] end of life and cold; no cell data sheet yet. Set it from the bench |
| `vsys-sense-top-ohm`, `vsys-sense-bottom-ohm` | 2.2 MΩ, 1 MΩ | R40, R41 |
| `imu-to-pod` | `0 1 0, −1 0 0, 0 0 1` | LSM6DSOX axes to the pod frame (pod = M × chip): U11 top side, pin 1 at the pod +x/+y corner, against ST's axis figure (chip +Y = pod +x). Checked to be a rotation at boot; [VERIFY] by the bring-up gravity test on all six faces |

| Flash (internal, 512 KB) | Offset | Size |
|---|---|---|
| MCUboot | 0x00000 | 32 KB (26.2 KB used) |
| Slot 0, app | 0x08000 | 220 KB (206.1 KB of the 216.9 KB the linker allows with swap-move) |
| Slot 1, update image | 0x3F000 | 220 KB |
| Settings and bonds (NVS) | 0x76000 | 8 KB, the two-sector minimum |
| Log | 0x78000 | 32 KB, 8 pages |

RAM use is 53.7 KB of 64 KB.

**Fitting 220 KB.** With logging the app is 252 KB. The release image turns logging off (32.5 KB), GATT robust caching, LE ping, controller privacy and the coded PHY (the nRF52832 has none) off, and uses the small printf: 206.1 KB. MCUboot fits 32 KB only with ECDSA P-256: the default RSA-2048 is 2.1 KB over and Ed25519 5.5 KB over, and serial recovery is 1.3 KB over (there is no UART for it anyway).

## Programming and debug

From `docs/flashing-and-test.md` of the baseline.

**Pads.** Four ENIG pads on the base reach SWD: VIN, GND, SWDIO, SWDCLK. Field updates are BLE only (section 9 of `docs/ble_gatt.md`); the SWD pads are the recovery path.

**Power on the jig.** VIN = 5 V through a 100 mA limit. The BQ21061 runs the system from VIN, so SWD works with a flat or missing cell.

**Probe.** Logic is 1.8 V. Use a probe that follows its target voltage (J-Link, or a DK's debug-out). The pod has no VTref pad: the jig feeds 1.8 V to the probe's VTref.

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
- APPROTECT: the build uses `NRF_APPROTECT_USE_UICR`. Debug stays open as long as UICR.APPROTECT reads HwDisabled after `recover` [VERIFY on the first silicon revision received].
- Production lock (to be decided before any release): `CONFIG_NRF_APPROTECT_LOCK=y` in the app and the MCUboot configs, then UICR.APPROTECT written at the end of the factory flash, so the log and the bond keys can't be read over SWD. `recover` still works, but it wipes them (`docs/ble_gatt.md` section 10).

**Logs.** The release image has none. For bring-up, `firmware/tests/fw.sh debug` builds the same app with SEGGER RTT logs (`src/debug.conf`). It is 224 KB without MCUmgr, more than the 217 KB a slot allows, so it runs without MCUboot from address 0 (`src/debug.overlay`), and has no firmware update; flash `build/debug/zephyr/zephyr.hex` over SWD and read RTT with `JLinkRTTClient`. Flash the release image again before testing updates.

**Updates after that.** MCUmgr SMP over BLE, as in `docs/ble_gatt.md` section 9.
- Upload `build/src/zephyr/zephyr.signed.bin`.
- There is no upload below 3.60 V (VSYS) off the dock.
- A new image confirms itself once it is healthy after 60 s. If it is still unhealthy at 300 s, it reboots, and MCUboot restores the previous image.
- The image is signed with MCUboot's development ECDSA P-256 key. Replace the key before any release [VERIFY].

## Power and charge logic

All of it is plain C in `src/power/`, stepped once a second by `src/main.c`.

| Rule | Implementation | Test |
|---|---|---|
| Cut-off at 3.30 V loaded, then System OFF | Three VSYS readings in a row below 3.30 V (read every 10 s), off the Plinth only. The log block closes, the optics and IMU shut down, the NTCs are off, and the nRF enters System OFF with /PG as its wake source. BQ21061 ship mode is not possible here: its I²C needs VIN (/LP is low). The AP6683 is the last guard | `test_cutoff.c`, `test_power_sm.c` |
| Low battery | Below cut-off + 150 mV, the mode drops to lite. It clears above cut-off + 250 mV [ASSUMED margins] | `test_cutoff.c`, `test_power_sm.c` |
| Dock detection | /PG low twice in a row to dock, high twice to undock | `test_dock.c` |
| Charger service | On every landing, after every VIN-good edge on /PG (the case's drop-outs), and every 10 s while docked (the BQ21061 I²C watchdog is 50 s and returns every register to reset): DEVICE_ID 0x3A checked, ICHG, the TS settings, CHARGER_DISABLE, then STAT0 / STAT1. Only with /PG low, so a service never lands inside a drop-out | `test_bq21061.c` |
| Charging case patterns | /PG edges in its interrupt (SENSE, not a GPIOTE channel: `sense-edge-mask`). A drop of 100–400 ms is a pulse; two within 2 s mark the pod fed by the case (Charge State flag bit 4) until a longer drop; three within 2 s reboot it from the interrupt, so it works with the threads hung (`src/power/vinpat.c`) | `test_vinpat.c`; the case tests run it against the case's own VIN output |
| Hardware temperature window | The cell NTC TH5 on the BQ21061 TS pin: no charging below 0.6 °C; 0.375 × ICHG (5.6 mA) from 0.6 to 10.3 °C; VBATREG − 150 mV from 40 to 45 °C; no charging above 45 °C (TS_HOT 0x38; the reset value would allow 60 °C). Thresholds from the 80 µA TS bias and the Murata table | `test_bq21061.c` |
| Firmware temperature window | The same window on TH4: charging disabled below 0 °C or above 45 °C; the cold current (5 mA) from 0 to 10 °C. TH4 sits next to the nRF and reads warm, so the hot stop errs early. No fresh TH4 reading (over 10 s old, or open) disables charging. 1 °C hysteresis | `test_charge_policy.c`: sweep from −10 to 60 °C |
| Charge state | Undocked, charging, full (STAT0 CHARGE_DONE), cold-limited, temperature-blocked (either window), fault (no I²C answer, VIN over-voltage or battery over-current) | `test_dock.c`, `test_power_sm.c` |
| Battery % (no gauge) | Coulomb count from the mode budgets, and ICHG while charging. Pulled towards the open-circuit voltage (VSYS + I × R) at light load, off the Plinth only (VSYS is 4.5 V on it). Snaps to 100 % on charge done | `test_soc_est.c` |
| LED headroom | VLED = VSYS. Below green-min-mv plus the pulse sag (15 mA × 2 Ω = 30 mV), green swaps to IR at the same current and the log marks the pulse as from IR. 50 mV hysteresis. Always green on the Plinth | `test_headroom.c` |
| Wear detection (#18) | Port of the ML reference models. Off-body when any of these holds:<br>- dark (DC below 0.15 of the active LED's on-skin reference) for 4 s;<br>- no pulse for 60 s while still and cool (TH1 below 30 °C or falling 0.1 °C/min);<br>- no pulse for 300 s below 32 °C.<br>Worn again when lit for 2 s and then a pulse within 15 s. One reference per LED, learned in the first pulse-present minute while worn, both cleared on the dock and at off-body. Without a reference for the active LED, the dark path is skipped and a pulse alone puts the pod back on. The worn state carries over the green-to-IR swap. Thresholds are the same for both LEDs and [ASSUMED] | `test_wear.c`, `test_ml_vectors.c` |
| Pair on lift | /PG rising opens 60 s for new pairings; the pod advertises throughout, so bonded phones reconnect at any time | — (needs a phone) |
| Docked idle | Optics unpowered (LDO_EN low), IMU off, NTCs at 1 Hz on the Plinth, advertising every 2 s | `test_modes.c` |
| Modes | The ML data contract: green at 25 Hz, accelerometer at 12.5 Hz, lite optics 30 s in every 120 s. Each LED at most once (MAXM86161) | `test_modes.c`, `test_maxm86161.c` |
| Research modes | Mode 6 / 7 (depth study, research only): green, IR, red and direct ambient (tag 0x04) at 100 / 200 Hz; modes 8 / 9 sample at 200 Hz and average 4 / 8 on the module (SMP_AVE) to 50 / 25 Hz; accelerometer at 104 Hz, 30 ms interval. One connection only, never stored; docked idle on the Plinth, full below the green limit. The 25 Hz filters get means of 4 / 8 samples (`docs/ble_gatt.md` 4.3) | `test_modes.c`, `test_power_sm.c`, `test_maxm86161.c`, `test_gatt_encode.c` |
| Vibration modes | Mode 10 / 11 (bench capture of bone-conducted voice and jaw vibration, research only): the LSM6DSOX accelerometer alone, high-performance, ±2 g, LPF2 off, at 3333 / 6667 Hz; LEDs, MAXM86161 and gyro off. FIFO mode, watermark 36 words, drained in 252-byte I²C bursts by a thread; the skin-normal axis (pod z) streams on Vibration Data 0x0207 with sequence, uptime and sample index. I²C at 400 kHz carries mode 10 but not mode 11 continuously, so mode 11 captures in windows with flagged gaps. 15 ms interval; asks for DLE and 2M. The same session rules as modes 6–9 (`src/vibcap.c`, `docs/ble_gatt.md` 4.4) | `test_vibcap.c`, `test_modes.c`, `test_power_sm.c` |
| Vibration bench rig | Not the pod: an nRF52 DK with the LSM6DSOX on SPI at 8 MHz runs mode 11 gap-free and prints each Vibration Data packet as `0207 <hex>` on the USB serial port at 1 Mbaud, for the voice study before EVT. `vibcap.c` unchanged; button 1 stops and prints a stats line (`tests/vib_bench`, `docs/ble_gatt.md` 4.5) | `tests/vib_bench/host` |
| Motion level and activity class | Port of the ML reference models. The LSM6DSOX program (97 register writes, ST's BSD-3 motion_intensity tree with Atrium's 7 thresholds) goes in after the driver's init and leaves the accelerometer at 12.5 Hz, ±2 g. Per epoch: still when every level is 0; walk at median level 4–5 with a step rhythm of 70–150 steps/min; run at level ≥ 6 and 140–200 steps/min; else other. Thresholds [ASSUMED] | `test_mlc.c` |
| Oxygen trend bursts | Port of the ML reference models. IR 10 mA and red 10 mA join the mode's green slot for 30 s at 25 Hz; green stays slot 0, so pulse and HRV run on. Gates [ASSUMED] | `test_spo2.c` |
| Pulse and HRV algorithms | Port of the ML reference models: spectral HR, 8 s, 256-point FFT, accelerometer subtraction; max-slope HRV at 25 Hz, RMSSD needs at least 5 clean differences | `test_ml_vectors.c` |

**Modes and budgets** (µA at the cell, [ASSUMED] until measured). The table in `src/power/modes.c` was built for the Rev B.2 3.3 V rail; on the EVT 1.8 V LDO rail the nRF and sensor currents need measuring again [VERIFY].

| Mode | Slots | Rate | Optics duty | Typical | Worst case |
|---|---|---|---|---|---|
| full | green 15, IR 10, red 10 mA | 25 Hz | always | 437 | 517 |
| smart (default) | green 15 mA | 25 Hz | always | 215 | 247 |
| lite | green 10 mA | 25 Hz | 30 s in 120 s | 70 | 80 |
| sleep | green 10 mA | 25 Hz | always | 129 | 154 |
| docked idle | none | — | — | 17 | 25 |
| off-body | green 5 mA | 8 Hz | always | 34 | 48 |
| research 6 | green 15, IR 10, red 10 mA, direct ambient | 100 Hz | always | 1512 | 1816 |
| research 7 | the same | 200 Hz | always | 2706 | 3286 |
| research 8 | the same | 200 Hz, averaged to 50 Hz | always | 2443 | 3010 |
| research 9 | the same | 200 Hz, averaged to 25 Hz | always | 2408 | 2973 |
| research 10 | none (accelerometer 3333 Hz, high-performance) | — | off | 1235 | 1518 |
| research 11 | none (accelerometer 6667 Hz, high-performance) | — | off | 1898 | 2587 |

## Log

`src/epoch.c`, `src/logpack.c`, `src/logstore.c`, `src/logxfer.c` (the download).

- **Epoch:** 30 s, day and night, starting at a 30 s step of uptime. Pulse is the mean of the seconds with a reported pulse; on skin means worn for more than half the epoch; steps are the step rhythm × 30 s in walk and run epochs [ASSUMED until counted]. An epoch spent wholly on the Plinth is not logged.
- **Record:** 12 bytes, byte for byte the reference encoder (round half to even).
- **Blocks and pages:** an 8-byte header (u32 first-epoch time, u16 sequence, u8 format 1, u8 count), up to 255 records, never across a 4 KB page. A new block at any gap in time, at 255 records and at a page boundary; a gap-free page holds 340 records. When the log is full, the oldest page is erased. 8 pages hold 19.8–22.6 h.
- **Time:** Unix seconds once the phone has set the clock; before that, seconds since boot with bit 31 set.
- **Download:** `src/logxfer.c` streams pages on Log Data and ends with a 0x90 indication. The head page goes out as it stood at the request (`logpack_snapshot()`), so a record logged mid-read is never half sent. `test_logxfer.c` runs whole syncs against a phone model of the app's PodHost and `decodeBlocks`. It covers oldest to head with a partial head page; records logged during the read, at every alignment and MTU (and, as a check, that the old unmasked read tears); a block closing mid-read; the ring dropping a page during a read; a disconnect and a resume from the cursor; busy, abort and bad requests; and uptime epochs rebased after a reboot. Each logged record is also sent on Live Epoch (0x0504).
- **Power loss:** a block's first record is written before its time, each record's last word (the "written" marker) before the rest, and the second header word only when the block closes. On boot the open block is closed, a record cut by the reset has its missing words set to 0, and anything else left past the last block ends the page. The head page, oldest page and next block sequence are saved in settings when a page opens. `test_logpack.c` resets at every 7th write of a 400-record run and checks that the log still decodes with `decode_blocks()` rules, with no byte written twice.

## Drivers

| Part | Driver |
|---|---|
| MAXM86161 | `drivers/maxm86161/` (`adi,maxm86161`). LDO_EN power gating, part ID 0x36 checked at each power-up, up to 3 LED slots and one direct-ambient slot at one rate, with optional on-chip averaging (SMP_AVE), FIFO drained on A_FULL, low-power mode between samples |
| BQ21061 | `drivers/bq21061/bq21061_regs.c` (register logic, `ti,bq21061`) and `src/hw/charger.c` (I²C and /PG) |
| LSM6DSOX | Zephyr `st,lsm6dso` (same register set), data-ready trigger on INT1, `gyro-odr` off in the devicetree. `accel-pm` low-power/normal (XL_HM_MODE = 1): low-power at 12.5 Hz, normal at 104 Hz, 1.8 mg RMS at ±2 g; the power-up high-performance mode is 170 µA at any rate (DS12814 Table 3). Modes 10 and 11 bypass the driver: data-ready off, raw writes for high-performance, FIFO and ODR, and back to low-power on the way out (`src/vibcap.c`) |
| LSM6DSOX MLC | `src/mlc.c` program, written over I²C after the driver's init; MLC0_SRC read on the embedded page, under a bus lock against the driver's ODR calls |
| NTCs, VSYS | SAADC, `src/hw/power_hw.c` |

## What needs hardware

1. **Bring-up and current.**
   - The baseline bring-up steps (docs/03): rails, then SWD at 1.8 V, then the debug image's RTT logs.
   - Docked-idle and every mode against the budget table, on the 1.8 V rail.
   - System OFF current with the MAXM86161, IMU and NTCs off, and the wake on /PG.
2. **Charger.**
   - DEVICE_ID 0x3A, ICHG 15 mA measured (13–17 mA, production step 5), CHARGE_DONE at the end.
   - The TS window on TH5 in a warm and a cold box, and TH4 against it: TH4 now reads the nRF, not the cell.
   - The I²C watchdog: no register reset while docked with the 10 s service.
   - Ship mode from Command 0x02 on the Plinth, entered at the lift, and the wake on the next Plinth (production step 7).
3. **VSYS_SENSE.** Against a meter on the cell and on the Plinth; the battery estimate against a discharge curve.
4. **NTCs.** TH1 and TH4 against ambient (±1.0 °C, production step 4); the NTC_EN pin drop and part tolerance set any offset.
5. **MAXM86161.**
   - The time from LDO_EN high to the first I²C access (2 ms assumed).
   - Multi-item FIFO burst reads.
   - Green Vf on VLED = VSYS, and the real green-min-mv.
   - Counts per mA on skin, green and IR.
6. **Cell.** Cell resistance against temperature and age, to set `cell-resistance-mohm` and confirm the cut-off margin.
7. **Wear detection.** The DC thresholds on skin, in air, on a table and in the case, each on green and on forced IR (VALIDATION.md V7).
8. **Pulse, HRV and posture.**
   - Pulse against a chest-strap reference, at rest and moving.
   - RMSSD against an ECG at night. Research mode 6 gives green at 100 Hz on the stream for O-18 [VERIFY against the O-18 protocol].
   - The PPG polarity: beats are taken at local maxima.
   - The accelerometer in low-power mode: its current at 12.5 and 104 Hz (the data sheet gives 26 µA at 50 Hz only), and the MLC levels and activity index against high-performance on the same motion (1.8 mg RMS against 70 µg/√Hz).
   - Research modes 6 and 7: no FIFO overflow at 200 Hz, no sequence gaps at the 30 ms interval on an iPhone and an Android phone, the direct-ambient level under room light and in the dark, and the current.
   - Vibration modes 10 and 11: the FIFO burst read rolls over from 0x7E to 0x78 (no skipped-word flag); mode 11's window and gap lengths; no index jumps on an iPhone and an Android phone; the analog anti-aliasing corner with a swept tone; the skin-normal axis and sign; the current.
   - The `imu-to-pod` matrix: rest the pod on each of its six faces and check +1 g on the pod axis pointing up (antenna end up reads +x).
   - These are wellness estimates only.
9. **Log.** A full night logged and downloaded, decoded with the log decoder; a reset during writing; the ring wrap after 22 h. With the app on an iPhone and an Android phone: the 15–30 ms interval granted during a read, the 0x90 end indication after the last Log Data packet, the time for 8 pages in sleep mode, and a Live Epoch every 30 s while worn.
10. **MCUboot.** A swap-move update over BLE end to end, and the revert of an unhealthy image.
11. **Radio.** Advertising and connection current, range on the head, MTU 247 and DLE with the app's phones; pairing only in the window after a lift.
12. **LFRC.** The calibrated LFRC holds 500 ppm across temperature, and BLE stays connected through a night.
13. **MLC.**
    - MLC0_SRC (0x70) on the embedded page [VERIFY].
    - Bench check: still on a table level 0, shaken 5–7, walking on the head 3–5.
14. **Oxygen trend.** R against a reference oximeter overnight (VALIDATION.md V5). No calibration and no percentage until V5.
15. **Charging case.**
    - The /PG delay after VIN drops and returns, against the 100–400 ms pulse window; a soft reset from the case reboots the pod, and its signature sets flag bit 4.
    - The idle current with the P0.14 edge interrupt on SENSE.
    - CTRL-AP RESET from the case with APPROTECT on, and the current after it (the debug power-up is released).
16. **SWD pads on skin.** Sweat between SWDIO, SWDCLK and the other pads (open item O-12): watch for leakage current in System OFF.
