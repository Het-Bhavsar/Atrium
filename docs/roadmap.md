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

## Next version

- **Custom cell.** A cell shaped to the shell, to bring the pod from 20.9 mm to around 17 mm long.
- **Voice.** Bone-conduction audio and a voice pickup for tap-to-talk with an AI assistant. Designed on paper, left out of EVT.
- **Deeper optics (research).** A source–detector spacing of about 3 cm (the fNIRS approach) is what reaches the brain; the
  pod's 2 mm spacing stays in the scalp. Needs a different form factor and validation, and makes no claim until then.
  **Help wanted**

## Signal and algorithms

- Sleep staging and recovery formulas validated on recorded nights. **Help wanted**
- Motion-artefact rejection for heart rate during walking and chewing. **Help wanted**
- Relative blood-oxygen trend (research only), with its limits stated.

## Openness

- Native KiCad versions of the schematics and boards. **Help wanted**
- OSHWA certification at the first public release.
- Translations of the app and the docs. **Help wanted**
