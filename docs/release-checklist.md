# Release checklist

Every Atrium release ties one hardware revision, one firmware revision and one app revision together, with the tests
run against that exact combination. A builder should be able to tell from this page which report applies to what they
hold. Fill in a new table for each release; never edit an old one.

## How to fill it in

1. **Hardware.** The board revision is the text on the bottom silkscreen (`ATRIUM EVT v0.1.0 2026-11` on the case,
   `EVT v0.1` on the flex). Record the Git commit of the released production files and the date of the DRC run.
2. **Firmware.** The pod's version comes from `firmware/src/VERSION`, and the build stamps the commit into the DIS
   Firmware Revision (`0.1.0+<commit>`). Bump a release in `VERSION` only: it sets both the MCUboot header the app
   checks before an update and the Firmware Revision the pod reports after it. `fw.sh build` stops if the two differ
   (`firmware/tests/check_version.py`), because a mismatch would show a good update as failed. Record which signing key
   signed the image: development key or the production key (never commit the key).
3. **App.** The version is `version:` in `app/android-ios/pubspec.yaml`, and the APK names on the release carry it.
4. **Tests.** Record each result with its date, and whether it was run on a host, in simulation or on real hardware.
   "Simulated" and "measured" are never mixed in one cell.
5. **Open items.** Every open item in [open-items.md](open-items.md) that affects the release is listed by number.

## Unreleased (since v0.1.3-evt)

| | |
|---|---|
| Hardware | (1) 14 November: charging case via holes 0.20 → 0.30 mm with the 0.45 mm pads unchanged, so the case builds as a standard JLC 2-layer board with no plugged-via or 4-wire-test surcharge; copper, mask, paste and silk Gerbers identical to v0.1.3-evt, drills and drill drawing changed. (2) 14 November: new `hardware/pod/islands-panel-1x2/`, the islands panel cut to 2 sets for a small EVT order. Silkscreen text unchanged |
| Firmware | Unchanged since v0.1.3-evt |
| App | On main since v0.1.3-evt (f833d13): OTA checked against the v0.1.3-evt release, demo runs the real update flow, onboarding redesign. Version still 0.1.2+3; no new APKs |
| Built units | None yet |

| Check | Where it ran | Result |
|---|---|---|
| Board DRC | EasyEDA, 2026-11-14 | Case: only J1's own pads to its own slots, as before. 1 × 2 panel: 0 errors, proven live with a planted short |
| Gerber check | Host, 2026-11-14 | Case: every copper, mask, paste and silk layer identical to v0.1.3-evt; holes 0.30 mm, hole edge to other-net copper ≥ 0.225 mm. 1 × 2 panel: identical to the 4 × 5 panel inside the frame opening on all copper, paste and silk layers and every drill file |
| Renders | Not regenerated | The case renders are unchanged: only the 81 via holes grew by 0.1 mm |

## v0.1.3-evt (EVT, 11 November 2026)

| | |
|---|---|
| Hardware | The v0.1.2-evt boards with the pre-fabrication changes since. (1) 10 November (O-48): radio RF match in Nordic's order (C3 shunt first), ANT1 far pad unconnected, case R19 330k lid pull-up fitted, flex JP6 tied to the flex ground pour. (2) 10 November (O-49): power island C50 / C52 now 10 µF 0402, charging case D4 VBUS clamp, R3 / R4 2.2 k, R25 SWDIO_DIR pull-down; schematics redrawn in titled blocks. (3) 11 November (O-50): ANT1 on Kinghelm's land pattern (0.70 × 0.90 mm pads at 1.875 mm pitch), a straight feed into pad 2 and the antenna keep-out reshaped to keep ground at x 18.76 mm; all 20 panel copies. Silkscreen text unchanged. Production files and the EasyEDA project at the commit tagged v0.1.3-evt |
| Firmware | Unchanged: pod 0.1.1 (MCUboot **development** key, bench units only) and case firmware 0.1.1. The same image files are attached to the v0.1.3-evt release, byte for byte (pod SHA-256 `864a3418…f7dd`), so the app's update check finds the pod image on the latest release |
| App | Unchanged at 0.1.2; the same APKs are attached to the v0.1.3-evt release |
| Built units | None yet |

| Check | Where it ran | Result |
|---|---|---|
| Board DRC | EasyEDA, 2026-11-10 and 2026-11-11 | Radio island and panel: 0 errors after the antenna change, proven live with a planted short on each. Power island: 0 (2026-11-10). Flex: re-run 2026-11-10 after the JP6 tie. Case: only J1's own pads to its own slots (0.171 mm against 0.2 mm), inside the vendor footprint |
| Gerber check | Host, on the exported Gerbers, 2026-11-11 | Radio island: no shorts or opens against the IPC-D-356A netlist; drills and every layer but top copper, top mask, top paste and top silk identical to v0.1.2-evt, and those change only at the antenna; JLC HDI rules met except the two O-47 vias. Panel: drills identical hole for hole, inner and bottom layers identical, top-layer changes equal to 20 × the radio change. BOM and CPL unchanged apart from ANT1's pad-1 position in the radio CPL |
| Mechanical fit | CAD, 2026-11-11, on the 10 November boards | Pod: no part clashes with the housing, cell, stiffener, flex or the other board; same open items as before (O-38 crush, courtyard near-misses). Case: no clashes; D4 and R25 clear; TH2 still 0.100 mm from the deck (O-46). The antenna change moves pads only. The copper-layer checks (optical-module pads, pogo pads) need the copper STEP exports and were not re-run |
| Datasheet review | 2026-11-11 | O-1 to O-4, O-6, O-7, O-22 to O-24, O-26, O-29 to O-31, O-33 closed with sources; O-50 opened and closed |
| Firmware and app tests | Not re-run | Unchanged |

Open items that affect this release: O-5, O-9 to O-21, O-25, O-28, O-32, O-34 to O-38, O-40, O-43 to O-47 in [open-items.md](open-items.md), and the build items in
[build-assembly-test.md](build-assembly-test.md#12-open-items).

## v0.1.2-evt (EVT, 10 November 2026)

| | |
|---|---|
| Hardware | The same boards as v0.1.1-evt (silkscreen unchanged), with the 2026-11-10 blind-via change on both islands and the panel: 12 of the 14 blind vias closer than JLC's 0.35 mm to the routed edge were moved (O-47), and the panel BOM now lists the released nRF52832-CIAA-G-R7, MAXM86161AEFD+T and LQM18DN100M70L. Flex and case unchanged. Production files and the EasyEDA project at the commit tagged v0.1.2-evt |
| Firmware | Unchanged from v0.1.1-evt: pod 0.1.1 (MCUboot **development** key, bench units only) and case firmware 0.1.1. The same image files are attached to the v0.1.2-evt release, byte for byte (pod SHA-256 `864a3418…f7dd`), so the app's update check finds the pod image on the latest release |
| App | 0.1.2 (version codes 2003 arm64 and 1003 armeabi-v7a; APKs on the v0.1.2-evt release, signed with the same key as 0.1.1, so they install over it). Raw signal recording (Settings, Research; off by default), raw light and motion export, and the real size under "On this phone" |
| Built units | None yet |

| Check | Where it ran | Result |
|---|---|---|
| Board DRC | EasyEDA, 2026-11-10, after the via change | Radio and power islands, panel: 0 errors. Flex and case: not re-run (unchanged) |
| Gerber check | Host, on the released Gerbers, 2026-11-10 | No shorts or opens against the IPC-D-356A netlists; copper and hole spacing at or above JLC's minimums; JLC HDI rules met except two radio vias at 0.289 and 0.291 mm from the edge (O-47); the 20 panel copies match the islands |
| Mechanical fit | Not re-run | Vias and tracks only; no outline or part moved |
| Pod and case firmware unit tests | Not re-run | Firmware unchanged: 177 of 177 and 48 of 48 at v0.1.1-evt |
| App unit tests and analysis | Host (`flutter test`, `flutter analyze`) | 179 of 179 pass, no analyzer issues |
| App on a device | iOS Simulator, demo build | Raw recording, the session list, export (light CSV, motion CSV, raw file) and delete work. Not run on an Android device or with a real pod |
| Raw stream from a real pod | Not run | Check on the bench, including the sleep window, where the 4 s connection interval may drop notifications (the session's missed count shows it) |
| Firmware update end to end | Not run | Needs a built pod (O-44) |
| Battery life, radio range, sensor accuracy | Simulation only / not run | As for v0.1.0-evt. The raw stream's battery cost (about 8 % by day, 14 % at night) is an estimate |

Open items that affect this release: O-36 to O-47 in [open-items.md](open-items.md), and the build items in
[build-assembly-test.md](build-assembly-test.md#12-open-items).

## v0.1.1-evt (EVT, 10 November 2026)

| | |
|---|---|
| Hardware | The same boards as v0.1.0-evt (silkscreen unchanged), with the 2026-11-10 part-spacing change on both islands (O-45) and the islands panel at 4 × 5, 83 × 71 mm. Production files and the EasyEDA project at the commit tagged v0.1.1-evt |
| Firmware | Pod 0.1.1 (`atrium-pod-0.1.1.bin`, SHA-256 `864a3418…f7dd`), signed with MCUboot's **development** key (bench units only). Case firmware at the tag: the fast over-current check now arms 10 ms after every VIN switch-on, the feed start and the signature and soft-reset pulses included |
| App | 0.1.1 (version codes 2002 arm64 and 1002 armeabi-v7a; APKs on the v0.1.1-evt release, same signing key as 0.1.0, so they install over it). OTA reports done only once the pod confirms the image; heart-rate gaps stay gaps; walks split on gaps over 2 min; Export data writes real CSVs |
| Built units | None yet |

| Check | Where it ran | Result |
|---|---|---|
| Board DRC | EasyEDA, 2026-11-10 | Radio and power islands, panel: 0 errors. Flex and case: known, accepted items only |
| Mechanical fit | CAD, 2026-11-10, on the released boards | No failures. Open items as in `mechanical/reports/`; new: TH2 courtyard at the 0.100 mm minimum to the deck (O-46) |
| Pod firmware unit tests | Host (`native_sim`, Twister) | 177 of 177 pass; `check_version.py` self-test 7 of 7 |
| Case firmware unit tests | Host (`make -C firmware/case test`) | 48 of 48 pass. In the host timing model a short is cut 30–50 ms after VIN comes on (it was 210–720 ms) |
| App unit tests and analysis | Host (`flutter test`, `flutter analyze`) | 163 of 163 pass, no analyzer issues |
| App on a device | iOS Simulator | Demo export shares a 183-line daily CSV; an empty store disables Export. Not run on an Android device or with real pod records |
| Firmware update end to end | Not run | Needs a built pod (O-44) |
| Short-circuit cut-off on a real dock | Not run | Oscilloscope on the first case; check the SY6280 turn-on against the 10 ms blanking (build item B-8) |
| Case charger temperature cut-off | Not run | TH2 against a thermocouple on the cell (O-46, B-10) |
| Battery life, radio range, sensor accuracy | Simulation only / not run | As for v0.1.0-evt |

Open items that affect this release: O-36 to O-46 in [open-items.md](open-items.md), and the build items in
[build-assembly-test.md](build-assembly-test.md#12-open-items).

## v0.1.0-evt (EVT, November 2026)

| | |
|---|---|
| Hardware | Islands panel, flex jumper and charging case as released in `hardware/`. EasyEDA project `hardware/easyeda/Atrium_EVT.epro2`. Since the tag: case charger BQ24040 with a cell-temperature cut-off (R5 2.7 k, R24, TH2), optical module MAXM86161AEFD+T, nRF52832-CIAA-G-R7 ordering code, LQM18DN100M70L inductor, 1N5819WS diodes, TPS61023DRLR, CL21A226MAYNNNE (see `docs/open-items.md` O-41) |
| Firmware | Pod 0.1.0, signed with MCUboot's **development** key (bench units only). Case firmware as in `firmware/case/` |
| App | 0.1.0 (APKs on the v0.1.0-evt release) |
| Built units | None yet |

| Check | Where it ran | Result |
|---|---|---|
| Board DRC | EasyEDA | Radio and power islands, panel: 0 errors. Flex and case: known, accepted items only (`hardware/README.md`). Case re-checked 2026-11-10 after the charger change: two USB-C footprint slot notes, nothing else |
| Pod firmware unit tests | Host (`native_sim`, Twister) | 177 of 177 pass |
| Case firmware unit tests | Host (`make -C firmware/case test`) | 45 of 45 pass, including the 50 ms short-circuit cut-off |
| Case charger temperature cut-off | Not run | Hot air and freeze spray on TH2, thermocouple on U2 (build item B-10) |
| App unit tests | Host (`flutter test`) | Pass; see the app README for the count |
| Firmware on hardware | Not run | No units built |
| Battery life | Simulation only | About 4 days (3.6–8 days simulated range); to be measured |
| Short-circuit cut-off on a real dock | Not run | Measure with an oscilloscope on the first case (build item B-8) |
| Radio range on the head | Simulation only | Link budget in `hardware/simulation/`; to be measured |
| Sensor accuracy | Not run | Heart rate, HRV, temperature and sleep need pod recordings against references |

Open items that affect this release: O-36 to O-42 in [open-items.md](open-items.md), and the build items in
[build-assembly-test.md](build-assembly-test.md#12-open-items).
