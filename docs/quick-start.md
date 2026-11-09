# Quick start

Five ways into Atrium, from "look at it on my phone" to "build a pod". Pick the one you need; each stands on its own.

| I want to | Go to | Needs |
|---|---|---|
| See the app | [1. Try the app](#1-try-the-app-no-hardware) | An Android phone |
| Read the design | [2. Open the files](#2-open-the-files) | A PDF viewer; optionally KiCad or Altium |
| Build the firmware | [3. Build the firmware](#3-build-the-firmware) | Docker |
| Make pods and cases | [4. Order and assemble](#4-order-and-assemble) | A PCBA account, a 3D printer |
| Flash and pair a pod | [5. Flash and pair](#5-flash-and-pair) | A J-Link or nRF DK, a WCH-LinkE |

## 1. Try the app (no hardware)

1. On an Android phone, open the [latest release](https://github.com/Het-Bhavsar/Atrium/releases/latest).
2. Download `atrium-0.1.0-demo-arm64-v8a.apk` (use `armeabi-v7a` on an older 32-bit phone).
3. Open it and allow the install from your browser when Android asks.
4. Start the app. The demo build runs on sample data, so every screen works without a pod.

The `pod` APKs are the same app talking to a real pod over Bluetooth. On iOS, build from source (step 3, App).

## 2. Open the files

```bash
git clone https://github.com/Het-Bhavsar/Atrium.git
```

- **No tools:** every board has a schematic PDF and a PCB PDF next to its sources (`hardware/*/*/schematic/`, `pcb/`); every
  part has a drawing PDF in `mechanical/*/drawings/`.
- **EDA:** the `.SchDoc` and `.PcbDoc` files open in Altium Designer, and in KiCad through File → Import.
- **CAD:** STEP files open in any CAD tool (FreeCAD, Fusion, Onshape); GLB files open in a browser 3D viewer.
- **Where to start reading:** [architecture](architecture.md), then [mechanical](mechanical.md) and
  [charging case](charging-case.md).

## 3. Build the firmware

Only Docker is needed on a Mac or Linux host; the toolchains live in images.

**Pod (nRF52832, Zephyr):**

```bash
firmware/tests/fw.sh image
```

```bash
firmware/tests/fw.sh test
```

```bash
firmware/tests/fw.sh build
```

The first builds the image once (about 10 minutes). `test` runs the unit tests; `build` writes
`firmware/build/atrium_pod_merged.hex` for SWD and `firmware/build/src/zephyr/zephyr.signed.bin` for updates over
Bluetooth. For production, sign with your own key: see [flashing and test](flashing-and-test.md#programming-the-nrf52832).

**Case (CH32V003):**

```bash
firmware/case/tools/case.sh image
```

```bash
firmware/case/tools/case.sh build
```

This writes `firmware/case/build/atrium_case.bin`.

**App (Flutter 3.47, Android and iOS)**, from `app/android-ios`:

```bash
flutter pub get
```

```bash
flutter test
```

```bash
flutter run
```

Add `--dart-define=ATRIUM_REAL_DATA=true` to `flutter run` to use a real pod instead of the demo data.

## 4. Order and assemble

1. **Pod boards:** order `hardware/pod/islands-panel/` (6-layer HDI, 0.82 mm, both islands on one panel, six pods per
   panel) as a two-sided PCBA. Read its [order notes](../hardware/pod/islands-panel/README.md#order-notes) first: cut
   the tabs, never snap them.
2. **Flex:** order `hardware/pod/flex-jumper/production/` as a bare 2-layer polyimide flex with ENIG and the SUS430
   stiffener. It has no parts.
3. **Case board:** order `hardware/case/charging-case/production/` as a one-sided PCBA with a 0.10 mm stencil.
4. **Parts not on the boards:** the 30 mAh cell, the case cell, magnets, spring pins and the skin pads are listed in
   [build, assembly and test](build-assembly-test.md).
5. **Enclosures:** print the STL files in `mechanical/pod/`, `mechanical/case/`, `mechanical/plinth/` and
   `mechanical/programming-jig/` for EVT fit checks; the drawings give the tolerances.
6. **Assemble** in the order of [build, assembly and test](build-assembly-test.md): join the islands with the flex, bring
   the pod up on the bench before the cell, fit the cell, close the shell, then run the end-of-line test.

## 5. Flash and pair

**Pod.** Put the pod in the programming jig (window end toward the arrow). The jig feeds 5 V on VIN and 1.8 V to the
probe's reference. Then:

```bash
nrfutil device recover
```

```bash
nrfutil device program --firmware firmware/build/atrium_pod_merged.hex --options chip_erase_mode=ERASE_ALL
```

**Case.** Before the cell is fitted, connect a WCH-LinkE to TP1 (SWIO), TP2 (3.3 V) and TP3 (GND) on the case board:

```bash
minichlink -w firmware/case/build/atrium_case.bin flash -b
```

```bash
minichlink -D
```

**Pair.** Install the `pod` APK (or run the app with real data). Put the pod on the Plinth or in the case, lift it off,
and pair from the app within 60 seconds: the pod only accepts new phones just after a lift, and just after it boots.

Details, limits and the production test are in [flashing and test](flashing-and-test.md).

## Stuck?

Open an issue with the matching template (bug, hardware issue, test report), or read
[CONTRIBUTING.md](../CONTRIBUTING.md) to send a fix.
