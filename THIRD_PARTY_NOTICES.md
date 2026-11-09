# Third-party notices

Atrium's own work is licensed as set out in the [README](README.md#licence): CERN-OHL-S-2.0 for hardware and mechanical
design, Apache-2.0 for firmware and the app, CC BY-SA 4.0 for documentation and images. Copyright 2026 Het Bhavsar.

This page lists everything in or used by the repository that someone else made, and the licence it comes under.

## Stored in this repository

| What | Where | Licence | Notes |
|---|---|---|---|
| LSM6DSOX machine-learning-core program (motion_intensity example), with Atrium's thresholds | `firmware/src/mlc.c` | BSD-3-Clause, © STMicroelectronics | ST's full notice is kept in the file header |
| Newsreader typeface | `app/android-ios/assets/fonts/` | SIL Open Font License 1.1 | Licence text in `OFL.txt` beside the font |
| Component symbols, footprints and 3D models | `hardware/easyeda/Atrium_EVT.epro2`, and the component bodies in the STEP files and renders | The terms of the EasyEDA / LCSC component library they came from | Atrium's own schematics, layouts and enclosure parts are CERN-OHL-S-2.0 |
| Results computed from the *pulse-transit-time-ppg* 1.1.0 dataset (PhysioNet) | `docs/research/brain-blood-flow/data/finger/` | Open Database License 1.0 | Per-record results only; the recordings are not stored here |

## Used to build, not stored here

| What | Used by | Licence |
|---|---|---|
| Zephyr RTOS v4.4.2 and the modules listed in `firmware/src/west.yml` (MCUboot, cmsis, hal_nordic, hal_st, mbedtls, tf-psa-crypto, segger, zcbor) | Pod firmware | Each under its own licence (Zephyr and MCUboot: Apache-2.0); see each module's licence file once fetched |
| Zephyr SDK 1.0.1 toolchain | Pod firmware build | Its own licences |
| [ch32fun](https://github.com/cnlohr/ch32fun) at `b28d285` | Case firmware | MIT |
| Ubuntu `gcc-riscv64-unknown-elf` | Case firmware build | GPL with the GCC runtime exception |
| Flutter SDK and the packages in `app/android-ios/pubspec.yaml` | Phone app | Each under its own licence; the app lists all of them under Settings > Open source |

## Data the app was trained on

| What | Licence | Notes |
|---|---|---|
| sleep-accel dataset 1.0.0 | Open Data Commons Attribution License 1.0 | Used to train the sleep and wake model (`app/android-ios/assets/models/sleep_v1.json`). Credited in the app under Settings > Open source |

## Images

The renders, layout plates and diagrams were made from Atrium's own CAD and board files. The photographs of people
wearing the pod are AI-generated illustrations made for this project; they show no real person. All of them are under
CC BY-SA 4.0 with the rest of the documentation.

## Components and trademarks

Atrium is built from catalogue parts made by other companies. Their chips are used, not copied: no part of their
internal design is in this repository, and no datasheet is redistributed. Each part is listed with its manufacturer
part number and LCSC number in the board BOMs, where its public datasheet can be found.

| Maker | Parts used in Atrium |
|---|---|
| Analog Devices (Maxim Integrated) | MAXM86161 optical module |
| Nordic Semiconductor | nRF52832 Bluetooth LE system-on-chip |
| STMicroelectronics | LSM6DSOX motion sensor |
| Texas Instruments | BQ21061 charger, TPS7A02 LDO, TPS61023 boost, SN74LVC1T45 level translators |
| WCH (Nanjing Qinheng) | CH32V003 microcontroller (charging case) |
| Silergy | SY6280 current-limited switch |
| Torex | XC6206 LDO |
| TPOWER | TP4054 charger (charging case) |
| Allpower | AP6683 cell protection |
| Hallwee | HAL248 Hall sensor |
| Kinghelm | KH-1608 chip antenna |
| NDK | NX1612SA 32 MHz crystal |
| Murata, Samsung Electro-Mechanics, Fenghua, Sunlord, YAGEO, UNI-ROYAL, TECH PUBLIC | Capacitors, inductors, resistors, ESD protection |
| XINGLIGHT, ALPS ALPINE, SHOU HAN | RGB LED, button and USB-C receptacle (charging case) |
| Sharvi | 30 mAh Li-po cell (pod) |

All product and company names are trademarks or registered trademarks of their owners. Their use here identifies the
parts only. None of these companies is affiliated with Atrium or endorses it.

## Names and marks

"Atrium", "Local Forge" and their logos are not licensed by any of the above (see the README). The OSHWA certification
mark is OSHWA's and will only appear once a certification UID has been issued.
