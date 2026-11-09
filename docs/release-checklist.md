# Release checklist

Every Atrium release ties one hardware revision, one firmware revision and one app revision together, with the tests
run against that exact combination. A builder should be able to tell from this page which report applies to what they
hold. Fill in a new table for each release; never edit an old one.

## How to fill it in

1. **Hardware.** The board revision is the text on the bottom silkscreen (`ATRIUM EVT v0.1.0 2026-11` on the case,
   `EVT v0.1` on the flex). Record the Git commit of the released production files and the date of the DRC run.
2. **Firmware.** The pod's version comes from `firmware/src/VERSION`, and the build stamps the commit into the DIS
   Firmware Revision (`0.1.0+<commit>`). Record which signing key signed the image: development key or the production key
   (never commit the key).
3. **App.** The version is `version:` in `app/android-ios/pubspec.yaml`, and the APK names on the release carry it.
4. **Tests.** Record each result with its date, and whether it was run on a host, in simulation or on real hardware.
   "Simulated" and "measured" are never mixed in one cell.
5. **Open items.** Every open item in [open-items.md](open-items.md) that affects the release is listed by number.

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
| Pod firmware unit tests | Host (`native_sim`, Twister) | 176 of 176 pass |
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
