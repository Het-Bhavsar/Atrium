# Contributing to Atrium

Thanks for helping. Atrium is open hardware, firmware and software, and every part of it can be improved: boards,
enclosure, firmware, the app, signal processing, testing and documentation. The [roadmap](docs/roadmap.md) marks the
places where help is most useful.

## Before you start

- Read the [README](README.md), [architecture](docs/architecture.md) and [open items](docs/open-items.md).
- For anything larger than a fix, open an issue first so the approach can be agreed before you spend time on it.
- Atrium is a wellness and research device. Contributions must not add medical claims or diagnostic features.

## Ways to contribute

| Area | Where | Notes |
|---|---|---|
| Firmware | `firmware/` | Zephyr / nRF Connect SDK for the pod, CH32V003 for the case. Build and test steps are in `firmware/README.md`. Every change keeps the unit tests passing and adds tests for new behaviour |
| App | `app/android-ios/` | Flutter. Run `flutter test` before a pull request. Follow the BLE interface in `docs/ble_gatt.md`; a change to the interface changes the doc in the same pull request |
| Hardware | `hardware/` | Open the `.SchDoc` / `.PcbDoc` in Altium Designer or import them into KiCad. Include updated PDFs, Gerbers, BOM and CPL with any board change, and say which checks you ran (DRC, impedance, stencil) |
| Mechanical | `mechanical/` | Include STEP, STL and an updated drawing PDF with any part change, and a fit check against the reference bodies |
| Testing | Issues | Bench results, wear tests and measurements are as valuable as code. Use the test-report template |
| Docs | `docs/` | Plain, short and factual |

## Pull requests

1. Fork the repository and create a branch from `main` (`fix/…`, `feat/…`, `hw/…`, `docs/…`).
2. Keep each pull request to one change, with a clear title and description. Fill in the template.
3. Write commit messages in the imperative ("Add …", "Fix …"), one subject line under 72 characters and a body that
   says why.
4. Make sure tests and checks pass, and attach evidence for hardware changes (DRC result, renders, measurements).
5. A maintainer reviews every pull request. Hardware changes need a second review before they merge.

## Rules for every contribution

- **Original work only.** Do not copy designs, layouts, firmware or images from other products, and do not include
  third-party datasheets or documents. Link to them instead.
- **Licences.** By contributing you agree that your work is licensed under the licence of the folder it goes into:
  CERN-OHL-S-2.0 for `hardware/` and `mechanical/`, Apache-2.0 for `firmware/` and `app/`, CC BY-SA 4.0 for `docs/`.
- **Parts.** New parts should be available from the LCSC / JLCPCB catalogue, with the LCSC number in the BOM.
- **Words.** The pod is worn on the "temporal region"; the no-glasses spot is the "temporal hollow".
- **Privacy.** Health data stays on the user's phone unless the user chooses otherwise.

## Community

Be respectful and constructive; see the [Code of Conduct](CODE_OF_CONDUCT.md). Report security issues privately as
described in [SECURITY.md](SECURITY.md).
