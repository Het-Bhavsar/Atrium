# Atrium app: Flutter blueprint

This is the plan for turning `design/prototype/` into the Flutter app. Feature numbers come from `docs/FEATURES.md`, and the pod interface is `docs/ble_gatt.md` (rev 13). The toolchain is the installed Flutter 3.47.5 (Dart 3.13.4); it is not upgraded.

## 1. App summary

Atrium is the phone companion for the temporal-region wellness pod. It pairs over BLE, guides the fit, and syncs the pod's 30 s log. It turns the log into heart rate, HRV, sleep, sleep head position, skin temperature, effort and recovery, all on the phone.
- **Platforms:** iOS and Android.
- **Data:** local-first, with no account (#23). There is no backend in EVT or V1; cloud features are Later and opt-in.
- **Demo mode:** runs on the prototype's sample data, so every screen works without a pod.

## 2. Design system (from the prototype CSS)

| Token | Light | Dark |
|---|---|---|
| ground / bg | #FFFFFF | #050506 / #0B0B0C |
| card / card-solid | #F4F4F6 / #FFFFFF | white 5.5 % / #18181A |
| ink / ink2 / ink3 | #111113 / #6B6B72 / #9D9DA4 | #F3EFE8 at 100 / 64 / 42 % |
| hair / track | black 9 % / 7 % | white 10 % / 9 % |
| button / button ink | #111113 / #FFFFFF | #F3EFE8 / #121212 |
| green (heart, HRV, Recovery) | #1E9E5A | #5FE39A |
| red (effort, warmth) | #D8452F | #FF6F5C |
| infrared, drawn violet (sleep) | #7A4FD6 | #B48CFF |
| cool | #2F6FD8 | #8AB8FF |

- **Type:**
  - Newsreader (serif, OFL, bundled, never fetched at run time) for headings and numbers: large title 40, section 24, story 23, cover number 76, tile number 34 light.
  - The system sans for everything else: body 16, secondary 14, caption 12.5, label 11.5 semibold tracked.
- **Shape:** card radius 22, button height 50 fully rounded, segmented control 30 high. Spacing runs on a 4-point scale: 4, 8, 12, 16, 20, 24, 32.
- **Motion:** 200 ms taps, 600 ms meters, 1.1 s gauge fill, all with the prototype's ease (0.2, 0.8, 0.2, 1).
- **Graphics, drawn with `CustomPainter`:**
  - the arch gauge (the Atrium mark, filled along its length);
  - the hypnogram, sparklines and the 24 h heart-rate chart;
  - charts drawn to match the prototype rather than a chart library's look.
- **Imagery:** `assets/img/` holds the per-page photos, the sky set and the CAD cutouts from `design/prototype/img/`.

## 3. Features (vertical slices)

| Slice | Screens | Features |
|---|---|---|
| `onboarding` | welcome, wellness notice, Bluetooth, lift off the Plinth, find and pair, pairing, glasses question, fit and side, signal and press, baseline | #15, #17, #21, #23, #24, #25, #26, #1, #18, #7 |
| `today` | Today (EVT and V1 variants, empty and learning states) | #1, #3, #13 |
| `metrics` | Recovery, Heart, Sleep (with head position), Effort, Skin temperature, Trends | #1–#7 |
| `device` | Device, measurement mode, skin pad, Plinth info, storage off, pod software | #16–#21, #25–#27 |
| `settings` | profile, notifications, data (Health, export, delete), Labs, about | #22, #23, #8–#12, #14 |
| `pod` (no UI) | BLE connection, log sync, settings writes, OTA | the transport behind everything above |
| `insights` (no UI) | ports of the ML reference models app-side modules | #2, #4, #5, #6, #7 |

## 4. Routes (`go_router`)

- **Onboarding:** `/welcome` → `/about` → `/bluetooth` → `/wake` → `/scan` → `/pair` → `/fit` → `/place` → `/signal` → `/baseline`. The guard sends anyone not yet paired here.
- **Tabs:** a shell with four tabs: `/today`, `/trends`, `/device`, `/settings`.
- **Pushed on top:** `/recovery`, `/heart`, `/sleep`, `/effort`, `/temperature`, `/pad`, `/firmware`, `/labs`.
- **Sheets:** `plinth-info`, `box`, `storage-off`, `fit-tips` and `mode`.
- **Deep links:** `atrium://pad` and `atrium://sync` for the notifications.

## 5. Domain entities (pure Dart)

- **Pod hardware:** `Pod` (id, name, firmware, hardware, wear side, mode), `ChargeState`, `FitQuality` (q, flags; fit good = bit 5).
- **Pod data:** `Epoch` (one 12-byte, 30 s log record: pulse, confidence, RMSSD, n_i, activity, pitch, roll, skin temperature, flags).
- **Derived per night:** `Night` (window, stages, head-position minutes, minutes on the pod, lowest HR, HRV), `Day` (resting HR, active minutes, zones, workouts).
- **Scores:** `Score` (Recovery, Sleep, Effort: 0–100, with a formula version), `Baseline` (usual ranges, 30 days).
- **User:** `SkinPad` (pads left, hours worn, side), `Profile` (age, height, weight, glasses).

## 6. State

Riverpod 3 with `riverpod_generator`:
- One `AsyncNotifier` per screen, reading use cases, plus `podConnectionProvider` (a stream), `syncProvider` and `settingsProvider`.
- Every screen handles loading, empty and error states.
- Repositories return `Either<Failure, T>` (fpdart).

## 7. Pod interface: native through Pigeon, plus existing SDKs

| Job | How | Why |
|---|---|---|
| Scan, pair, GATT reads and writes, notifications, Fit Quality stream | **Pigeon** API `PodHost` → Swift (CoreBluetooth) and Kotlin (Nordic Android-BLE-Library) | One BLE stack that also runs in the background |
| Background log sync at least every 16 h (#20) | Same native layer: iOS state restoration and background mode; an Android foreground service with a companion-device association. It fills the log into the same database | The sync must happen without the app open |
| Firmware update (#21, MCUmgr SMP) | **`mcumgr_flutter`** (Nordic's own plugin over its iOS and Android Device Manager libraries) | An existing official SDK; no need to wrap it ourselves |
| Health (#22) | **`health`** package (HealthKit, Health Connect) | Existing SDK |
| Local store | **drift** (SQLite) | Typed queries over epochs and nights |

Pigeon definitions live in `pigeons/pod.dart`; the generated Swift, Kotlin and Dart are committed.

## 8. Dependencies (current on pub.dev, all Dart 3.13 compatible)

- **Runtime:** flutter_riverpod 3.5, riverpod_annotation 4.0, go_router 18.1, freezed_annotation 3.1, json_annotation, fpdart 1.2, drift 2.35 + drift_flutter 0.3, mcumgr_flutter 0.10, health 13.3, permission_handler 13.0, share_plus 13.3, intl 0.20, flutter_localizations.
- **Dev:** pigeon 29.1, build_runner 2.17, riverpod_generator 4.0, freezed 4.0, json_serializable 6.15, drift_dev, very_good_analysis 11.0, mocktail.
- **Not used:**
  - dio and retrofit: there is no backend.
  - google_fonts: the font is bundled, so nothing is fetched.
  - flutter_blue_plus: BLE goes through Pigeon, so there is one stack.

## 9. Ports from the ML reference models (app side)

Each is ported to pure Dart in `insights/` and checked against the reference models' test vectors in `test/fixtures/` (the reference models themselves are not part of this release):

| Reference model | Feature | What is ported |
|---|---|---|
| Log record | log | The 12-byte epoch decoder |
| Head position | #5 | Sleep head position, plus `on_pod` |
| Sleep | #4 | 6 features and the logistic regression |
| HRV | #2 | The nightly summary |

## 10. Risks and gaps

1. **Package id:** `open.atrium` (owner, 2026-11-07), on iOS and Android. Check it is free on both stores before the first upload.
2. **Formulas:** the Recovery (#7) and Effort (#6) formulas aren't written by ML yet. The app shows sample scores until they are.
3. **Hardware:** no EVT pod exists yet. The BLE layer is built against `ble_gatt.md` and tested with an nRF52 DK running the firmware, or a mock `PodHost`.
4. **iOS background BLE:** it is best effort (state restoration, no guaranteed wake). The 16 h warning on Today and Device (#20) covers it.
5. **Android:** a foreground service needs a visible notification while syncing, and Health Connect needs its own permission screen.
6. **Localisation:** strings are English only to start; the ARB is set up for more.

## 11. Folder map

```
app/android-ios/
├── design/                  prototype and README (unchanged)
├── pigeons/pod.dart         Pigeon contract
├── lib/
│   ├── core/                theme (tokens), routes, errors, di, widgets (arch, charts, cover, cards)
│   ├── features/<slice>/    data / domain / presentation per section 3
│   └── l10n/
├── ios/ android/            Flutter shells + native PodHost (Swift / Kotlin)
├── assets/                  img, fonts (Newsreader, OFL)
└── test/                    unit (ports vs test vectors), widget, golden
```
