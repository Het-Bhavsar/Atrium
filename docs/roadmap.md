# Roadmap

Where Atrium goes after EVT, and where help is most useful. Items marked **help wanted** are good places to start.

## EVT: build and measure

- Order the islands panel, flex and case; assemble the first 10 pods and 5 cases.
- Bring-up: rails, radio, optical module, IMU, charging, case feed (`docs/flashing-and-test.md`).
- Tune the antenna match on a VNA in the closed pod.
- Measure battery life in Full, Smart and Lite modes.
- Measure heart rate against a chest strap at rest and walking; publish the result. **Help wanted**
- Skin-pad wear test: five people, both sides, a night and a day. **Help wanted**

## DVT: make it repeatable

- Production test jig and an end-of-line test that writes a calibration record.
- Injection-moulded shell and base; NCVM finish trials.
- Cell supplier drawing with tolerances and a welded tab.
- Firmware: signed OTA updates, debug lock for production.
- App: background sync on Android and iOS, Health Connect and HealthKit export. **Help wanted**

## Signal and algorithms

- Sleep staging and recovery formulas validated on recorded nights. **Help wanted**
- Motion-artefact rejection for heart rate during walking and chewing. **Help wanted**
- Relative blood-oxygen trend (research only), with its limits stated.

## Openness

- Native KiCad versions of the schematics and boards. **Help wanted**
- OSHWA certification at the first public release.
- Translations of the app and the docs. **Help wanted**
