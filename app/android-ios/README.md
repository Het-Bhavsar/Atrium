# Atrium app (Flutter)

Atrium is made by Local Forge ([localforge.in](https://localforge.in)).

<p align="center">
  <img src="../../docs/images/app/app_daily.jpg" alt="App screens" width="100%">
</p>


The phone app for the Atrium pod: pairing, fit guide, Today, Recovery, Heart, Sleep (with sleep head position), Effort, Skin temperature, Trends, Device, Skin pad, Pod software and Settings.
- **Bundle id:** `open.atrium` on iOS and Android.
- **Design:** `design/` (the clickable prototype and its README).
- **Plan:** `BLUEPRINT.md`.
- **Features:** numbered as in `docs/FEATURES.md`.
- **Pod interface:** `docs/ble_gatt.md`. The app is built to rev 13; revs 14–16 add only research modes and the charging-case flag, so it works with the rev 16 firmware.

Wellness and research only. No screen makes a medical claim.

## Run

Uses the installed Flutter stable (3.47, Dart 3.13). From `app/android-ios`:

```
flutter pub get
flutter test
flutter run                                       # starts at onboarding
flutter run --dart-define=ATRIUM_START=/today     # jump straight to a screen
flutter run --dart-define=ATRIUM_REAL_DATA=true   # real pod data instead of the demo set
```

### Demo or real data

One build switch, `ATRIUM_REAL_DATA` (`lib/core/config.dart`), picks the data:

| | Demo (default) | Real (`--dart-define=ATRIUM_REAL_DATA=true`) |
|---|---|---|
| Metrics | The prototype's sample set (`sample_data.dart`). It uses the same seeded RNG, so the numbers match the prototype; `test/sample_data_test.dart` checks this. | `PodMetrics`, computed from the synced log (`pod_metrics.dart`) |
| Pod status | The demo situations | Live Charge State notifications |
| Pairing, fit guide | Simulated | Real scan, pair, clock set, Fit Quality (0x0206) and heart rate (0x2A37) |
| Device | Simulated | Mode writes 0x0601 and ship mode writes 0x0605; Search again reconnects; last sync time and the pod's software version (DIS 0x2A26) |
| Today | Static | Pull to sync the log; last sync time, and "Sync soon" once it is over 16 h old |
| Pod software | A simulated run, labelled "Demo" on screen; it never says "updated" | The latest signed GitHub release, sent over MCUmgr SMP (see "Pod software update") |
| Sleep, oxygen trend (Labs) | Hidden | With Labs on and 7 earlier nights: last night's red/IR ratio as a multiple of the user's own baseline, never a percentage |

Synced epochs are kept on the phone in `epochs.bin` (`epoch_store.dart`), which re-decodes with the log decoder. The time of the last sync is kept beside it (`epochs.bin.synced`), so Today and Device show its age after a restart or out of the pod's reach. Recovery, Sleep score and Effort show the learning state, because their formulas don't exist yet. Real builds show only plain sentences about what the log holds, not the demo stories.

### Phone-side algorithms

The pod computes pulse, per-epoch RMSSD, wear, activity, steps, fit quality, pitch and roll, skin temperature and oxygen bursts. The phone computes what needs a whole night, several nights, the wear side or local time. These are ports of the ML reference models:

| Step | Dart |
|---|---|
| Night window, rest run, 30 s grid | `nightly.dart` |
| Sleep/wake (logistic model), stages (Labs) | `sleep_model.dart` with `assets/models/sleep_v1.json` |
| Sleep period, wake-ups, resting HR, pooled RMSSD, skin temperature, positions, oxygen ratio | `nightly.dart` (`summarise`) |
| 14-night baseline and deviation (5 nights minimum) | `nightly.dart` (`deviation`) |
| Upright calibration, restarted at each pad change | `nightly.dart` (`uprightFromLog`), `AppState.padChangedAt` |
| Fit press (8 of 10 s) and guided side tilt | `pod/domain/fit_checks.dart` |
| Recovery, Sleep score, Effort zones and walks/runs (Atrium's own, `scores_v0`, [ASSUMED] constants) | `insights/domain/scores.dart`, `pod_metrics.dart` |

**Which models run on which phone** (`insights/domain/model_plan.dart`): the pod already computes pulse, pulse quality and per-epoch RMSSD; the phone pools them. Its own models are the two logistic models in `sleep_v1.json`. Sleep/wake runs on every phone; the Labs stage model runs only with Labs on and not on low-end phones (under 3 GB RAM or Android's low-RAM flag, `insights/data/device_tier.dart`). No research model is bundled. Every tier computes off the UI isolate (`compute`); the tier sets how often live readings recompute (2, 5 or 15 min) and the history is bounded to 200 days (the six-month range plus baselines).

**Scores** (`scores.dart`): Recovery = HRV (50 + 25 z, weight 0.35) and resting heart rate (50 − 25 z, 0.25) against the median of up to 14 earlier nights (spread = max(1.4826 MAD, 3 ms or 2 bpm)), sleep against the usual (0.25) and skin temperature (100 − 125 |Δ°C|, 0.15); no score before 5 nights. Sleep score = 0.5 duration (of 8 h) + 0.3 efficiency + 0.2 continuity (−15 per wake-up). Effort = 100 (1 − e^(−L/150)), L = minutes in five heart-rate-reserve zones (Karvonen; maximum 208 − 0.7 age, or 190 without an age) weighted 1–5. Wellness readings, tuned on EVT and V1–V4 nights later.

`test/nightly_test.dart`, `test/pod_metrics_test.dart` and `test/fit_checks_test.dart` check the port against the ML vectors in `test/fixtures/phone` and `test/fixtures/fit`: features and P(asleep) within 1e-6, minutes and counts equal, bpm, ms and °C within 0.01. The model is data: a retrained `sleep_v1.json` ships without a code change. It was trained on 31 wrist nights (sleep-accel 1.0.0, ODC-By 1.0, credited under Settings > Open source), so sleep is shown as an estimate until it is retrained on pod nights. Every threshold in `NightConfig` is [ASSUMED].

### Privacy: no account, no cloud

Atrium has no accounts and no server: the phone talks to the pod directly over Bluetooth.
- Onboarding asks for a name, age, height and weight. They're saved in `profile.json` in the app's private folder, and Settings edits them.
- Readings live in `epochs.bin` beside it.
- Both are encrypted at rest by the OS (iOS Data Protection, Android file-based encryption).
- **Export data** (Settings) writes CSVs from what is stored on the phone and opens the system share sheet; the app sends nothing itself. Columns below, under "Data export".
- **Log out** (Settings, at the bottom) clears this phone: it forgets the pod (`forgetPod`), deletes the readings and the profile, resets settings except the theme, and returns to Welcome. **Delete all data** removes only the readings; **Forget this pod** (Device) only unpairs.
- The only network request is the pod software check: opening Device > Pod software fetches the latest release of `Het-Bhavsar/Atrium` from the GitHub API and, if you update, its `atrium-pod-<version>.bin` asset. Nothing about you or the pod is sent. Android's release manifest declares `INTERNET` for it.
- Both are kept out of backups: iOS marks the folder excluded from iCloud and computer backup (`FilesHostApi.keepOnDevice`); Android turns backup and device transfer off in the manifest.
- The Bluetooth link's security (LE Secure Connections pairing, an encrypted link, pairing only just after a lift from the Plinth) is set by the pod firmware; see `docs/ble_gatt.md`.

### Deep links (debug builds)

`atrium:///<route>?<state>` opens any screen in any demo state. The keys are the prototype's: `build=evt`, `dev=charging|full|cold|syncdue|notfound|offskin`, `data=empty|learning`, `glasses=yes|no`, `labs=1`, `sheet=box|plinth-info`. Add `y=<pt>` to scroll. Example:

```
xcrun simctl openurl booted "atrium:///today/sleep?dev=charging&y=700"
```

This is how the HTML-vs-Flutter comparison is shot. The comparison goes to `design/compare/`, which git ignores because it holds pod imagery.

## Motion and code style

- **Motion:** shared tokens in `Motion` (`tokens.dart`).
  - Tabs fade in when chosen (`FadeIndexedStack`).
  - Page content eases in (`Reveal`).
  - Gauges sweep and their numbers count up; cover numbers count up too.
  - Charts draw in, and redraw when the range changes.
  - The segmented thumb slides.
  - Tabs, segments and chart scrubbing give a light haptic tick.
- **Reduce Motion:** with the system setting on (iOS Reduce Motion, Android "Remove animations"), everything lands at once (`Motion.of`, `Motion.reduced`).
- **Code:** formatted at 160 columns (`formatter.page_width` in `analysis_options.yaml`). Lints are `flutter_lints` plus package imports, ordered directives, final locals, unawaited futures, and strict casts and raw types. Run before committing:

```
dart format lib test pigeons
flutter analyze
flutter test
```

## Layout

| Path | What |
|---|---|
| `lib/core/theme/` | Design tokens (colours, type, spacing, radii, motion) and the theme |
| `lib/core/widgets/` | Arch gauge, charts, hypnogram, cards, list rows, cover page, tab shell |
| `lib/core/app_state.dart` | App state (Riverpod) |
| `lib/core/routes/` | `go_router` table |
| `lib/features/onboarding/` | Welcome to Baseline (#15, #17, #23–#26) |
| `lib/features/today/` | Today (#1, #3, #13) |
| `lib/features/metrics/` | Recovery, Heart, Sleep, Effort, Skin temperature, Trends (#1–#7) |
| `lib/features/device/` | Device, Skin pad, Pod software, sheets (#16–#21, #25–#27) |
| `lib/features/settings/` | Settings and Labs (#8–#12, #14, #22, #23) |
| `lib/features/pod/` | Pod entities, the log decoder and the Pigeon API |
| `pigeons/pod.dart` | Pigeon contract; regenerate with `dart run pigeon --input pigeons/pod.dart` |
| `ios/Runner/PodHost.swift` | CoreBluetooth host with state restoration (background mode `bluetooth-central`) |
| `android/app/src/main/kotlin/open/atrium/PodHost.kt` | Android BLE host (serialized GATT operations) |
| `test/` | Checks against the Python references: the log decoder against the reference log vectors and head position against the reference vectors. Also: `PodMetrics` on a synthetic log, every screen with an empty pod log, and demo data against the prototype |

## Native pod link

Flutter talks to the pod through Pigeon (`PodHostApi`, `PodFlutterApi`):
- **Native side:** scans for the Power service (0x0100), connects, bonds, reads, writes and notifies by the 16-bit ids in `ble_gatt.md`. It also downloads the log (Log Control 0x0502 op 0x01, pages on 0x0503) and finishes on the pod's end indication (0x90 on 0x0502), with a 10 s idle fallback for disconnects and older firmware. Android's GATT client is `PodGatt.kt`, shared by the app's link (`PodHost.kt`) and background sync (`PodSyncWorker.kt`).
- **Bonding and reconnect:** Android waits for the bond before a connect completes (the pod drops links still unencrypted after 30 s), and stores the address that answered. The pod changes its private address every 15 min, so when a bonded pod doesn't answer at the stored one, an 8 s filtered scan finds it by the bond. iOS keeps a pending connect after a drop, which CoreBluetooth completes the moment the pod advertises. Dart retries with back-off (1, 2, 5, 10, 30, 60, 120 s) while the app is in front, and at once on resume.
- **Link speed:** MTU 247 and the 2M PHY are asked for on Android; the connection runs at high priority only during a log download, then back to balanced. iOS follows the pod's own connection parameters (its interval follows the measurement mode, `ble_gatt.md` section 2).
- **Realtime:** after each connect the app subscribes to Live Epoch 0x0504, written to the store in batches of four (a lost batch is re-read by the next sync, since only a full sync moves the cursor); screens recompute at most as often as the model plan says. A head page more than one past the sync cursor starts a sync. Heart Rate Measurement 0x2A37 is subscribed only while the app is in front (Today and Heart show it live). The fit guide turns on the raw PPG stream (PPG Control 0x05) and draws the pod's own green samples from PPG Data 0x0202; with no samples it draws only the baseline.
- **Uptime records:** records stamped with pod uptime (bit 31, after a reboot) wait for a sync, which moves the current boot's run onto Unix time. Known gap [VERIFY]: a previous boot that ended at a lower uptime than the current boot's first record is joined to the current run and mis-dated; the log has no boot marker.
- **Dart side:** decodes the log pages with `log_record.dart`, a port of the log reference decoder checked bit for bit against it. More than two pages decode on another isolate, as do a large store at launch and the metrics.
- **State:** one source of truth per thing. `podSessionProvider` holds the link phase (`pod/domain/link_machine.dart`), the sync (`sync_runner.dart`: one at a time, requests during a sync fold into one more run), the last Charge State, the firmware revision and the live pulse; `podStatusProvider` derives the pod card from it. The update's phases live in `firmwareUpdateProvider`, which borrows the link (`handOver`, `takeBack`). Timers, subscriptions and syncs stop on dispose.
- **Firmware update:** `mcumgr_flutter`, Nordic's own plugin (see below).
- **Health:** the `health` package.

## Background sync (#20)

The pod keeps about 20 h. While the app is closed:
- **Android:** a WorkManager job (`PodSyncWorker`) every 4 h connects to the bonded pod (skipped while the app holds the link), sets its clock, reads the log from the app's cursor and leaves the raw pages in `pod_spool/` beside the store. Where the system allows it the job runs as a short foreground service of type connectedDevice. Device shows a notice when battery optimisation may hold it back, with a link to the system list (no direct exemption request).
- **iOS:** `bluetooth-central` and state restoration. After a drop the app keeps a pending connect; when the pod comes in range iOS wakes (or relaunches) Atrium, and `PodHost.swift` downloads the log itself if the app isn't in front. Device says so when Background App Refresh is off.
- **Both:** native only moves bytes (`pod/data/spool.dart` has the file format). The app takes the files in at launch and on every resume, decoding them off the UI isolate.

## Storage

Hive (`hive_ce`, `core/storage/local_store.dart`) holds the settings (kept across launches; someone who finished onboarding opens on Today), the profile (moved over from the old `profile.json`) and the last computed metrics (so a launch opens with numbers while the log is read). The raw readings stay in `epochs.bin`: 16-byte rows in the exact log codec, read in one pass, with the cursor beside it that native background sync reads and moves. Everything is in the app's data folder, kept out of backups; log out and "Delete all data" empty it.

## Data export (#22)

Settings > Export data writes CSV files from this phone's store and hands them to the system share sheet (`share_plus`); where they go is the user's choice. The sheet shows the real span of stored data, first to last day. "30-second records" is a switch, on by default when there are records. The demo build exports its demo days and has no 30-second records. With nothing stored, the sheet says so and writes no file. Code: `features/settings/data/data_export.dart`.

Files are comma-separated, UTF-8, with one header row; an empty cell means the value isn't known.

**`atrium-daily-YYYYMMDD.csv`**: one row per local day, oldest first, the values the screens show.

| Column | Unit | Meaning |
|---|---|---|
| `date` | YYYY-MM-DD, phone's time zone | The day |
| `recovery_score` | 0–100 | Recovery; empty before there is a baseline |
| `sleep_score` | 0–100 | Sleep score; empty with no night |
| `effort_score` | 0–100 | Effort, from heart-rate zones |
| `resting_hr_bpm` | beats per minute | Resting heart rate of the night |
| `hrv_rmssd_ms` | ms | Night HRV (RMSSD) |
| `asleep_min` | minutes | Time asleep |
| `skin_temp_deviation_c` | °C | Night skin temperature against your usual; empty with no night |
| `steps` | count | Steps from the pod |
| `active_min` | minutes | Minutes walking or running |

**`atrium-records-YYYYMMDD.csv`**: the pod's 30-second records as stored in `epochs.bin`, oldest first. Every field of the log record (`pod/data/log_record.dart`, the firmware's compact log).

| Column | Unit | Meaning |
|---|---|---|
| `time_unix_s` | s | Start of the 30 s epoch, Unix time (UTC) |
| `time_utc` | ISO 8601, UTC | The same time, readable |
| `pulse_bpm` | beats per minute | Pulse; 0 = none |
| `pulse_confidence` | 0–1 | Pulse quality (4-bit, in steps of 1/15) |
| `on_skin`, `pulse_from_ir`, `docked`, `charging` | 0 or 1 | Flags: on skin, pulse from the IR channel, on the Plinth, charging |
| `rmssd_ms` | ms | HRV (RMSSD) over the epoch; 0 = fewer than 5 clean beat differences |
| `clean_diffs` | count | Clean beat-to-beat differences behind it |
| `activity` | still, walk, run, other | Activity class |
| `activity_mg` | milli-g | Motion intensity |
| `mlc_level_max` | 0–7 | Highest motion level from the motion sensor's own classifier |
| `pulse_present_s` | s | Seconds of the epoch with a pulse |
| `skin_temp_c` | °C | Skin temperature; empty = not valid |
| `steps` | count | Steps in the epoch |
| `pitch_deg`, `roll_deg` | degrees | Pod orientation; empty = unknown |
| `burst_ratio_r` | ratio | Red/IR ratio of an oxygen burst ending in this epoch; 0.00 = none. A research value, not a blood-oxygen percentage |
| `burst_confidence` | 0–1 | Quality of that burst |
| `record_hex` | hex | The 12 record bytes as logged, to decode again |

## Sync status (#20)

The pod's log is 8 pages of 4 KB, about 20 h, and the oldest page is erased when it is full; the battery lasts days longer. So real builds show "Last synced …" on Today (under the story, and in the empty state) and on Device (caption, Last sync row, and in the not-nearby card), and a "Sync soon" notice once the last sync is more than 16 h old: "sync soon or older readings will be overwritten" (`pod/domain/sync_age.dart`, `pod/presentation/sync_status.dart`). A full sync and live records (Live Epoch 0x0504, saved at most every 5 min) both count as synced. `docs/FEATURES.md` #20 still says the app warns after 12 h; the app uses 16 h, 4 h before the log starts overwriting.

## Pod software update (#21)

Real builds (`device/domain/firmware_update.dart`, `device/data/`, `device/presentation/firmware_view.dart`):

1. **Check.** The pod's version is DIS Firmware Revision 0x2A26 (`0.1.0+<commit>`, read at every connect). The latest release is `GET https://api.github.com/repos/Het-Bhavsar/Atrium/releases/latest`, asset `atrium-pod-<version>.bin` (`zephyr.signed.bin`). Newer than the pod: Update; otherwise Up to date. No release, or the repository not public: "No pod software has been published yet."
2. **Gate.** On the Plinth (or in the case) it may start. Off it, the pod refuses below 3.60 V VSYS (ble_gatt.md section 9), so the app reads VSYS from Charge State bytes 6–7 and refuses below 3.60 V or with the low-battery state; with no voltage reading it asks for the Plinth.
3. **Download and check the file.** Size within 216.9 KB, MCUboot header magic, the TLV area, the image's own SHA-256 (TLV 0x10, over the header, image and protected TLVs) against its bytes, GitHub's asset digest when the release has one, and the header's version equal to the asset's. A file that fails any of these is never sent.
4. **Hand-over.** Native code owns the app's BLE link, and `mcumgr_flutter` opens its own (iOS: its own `CBCentralManager` to the same peripheral UUID; Android: its own GATT client to the same address). `PodSession.handOver` waits for a running sync, moves the link phase to handed over (no reconnects, syncs, live records or writes) and calls `PodLink.release`. The phone is already bonded (pairing completes before any connect does), so MCUmgr's link is encrypted, which the SMP characteristic needs.
5. **Slots, then upload.** The slot list is read first: an image already running and confirmed is not sent again, and a trial still running from an earlier update blocks a new one. `McumgrTransport` uploads with the verified hash, mode `testOnly`, `eraseAppSettings` off (the settings partition holds the bonds), a 60 s stall timeout; a dropped link is retried twice after 5 s (the pod resumes a matching image [VERIFY]). The pod's refusal below 3.60 V (MGMT_ERR_EBUSY) and pairing errors become plain words.
6. **Confirm.** The app never confirms an image. After a 10 s swap wait [ASSUMED] it reads the slot list every 10 s (6 min at most) until the pod's own health check has confirmed the new image (`firmware/src/dfu_health.c`, 60 s) or reverted it (300 s or a reset): a revert is shown as such. If the slot list can't be read at all, the app waits about 90 s from the reset (past the 60 s check) before the version check.
7. **Verify.** `PodSession.takeBack` reconnects, reads 0x2A26 again and resumes syncing. Only a confirmed image on a pod that reports the release's version shows "Updated".
8. **Pending.** A pod that reports the new version while its slot list never said "confirmed" is "Installed, confirmation pending", which is neither done nor failed: a trial image reports its version too. Every 60 s the app lends the link to MCUmgr again, reads the slot list, then reconnects and reads the version. It ends "Updated" (confirmed), as a revert (slot list reverted, or the pod back on another version), or, past 6 min from the reset (beyond the pod's 300 s revert), "Installed, not confirmed yet" with a button to check the pod again.

Native changes for this (small, both platforms): `disconnect()` now also cancels a connect that is still pending and fails its waiter, and a dropped link fails the read, write and connect calls still waiting, so the retry loop and the hand-over never hang (`ios/Runner/PodHost.swift`, `android/.../PodHost.kt`).

Demo builds run a timer instead, say "Demo" on every state, and finish with "Demo finished. Nothing was sent to a pod." No version numbers are written into the app.

Picking a local `.bin` file isn't offered: it needs a file-picker plugin (native code on both platforms) the app doesn't carry yet.

## Build notes

- **iOS:** Swift Package Manager resolves Nordic's `ios-nrf-connect-device-manager` to 1.14.9. `mcumgr_flutter` 0.10.1 doesn't compile against 1.15, so keep the pin in both `Package.resolved` files.
- **Android:** compiled against API 37 (`android-37.0`) with minSdk 26 (Health Connect). `android/build.gradle.kts` builds `mcumgr_flutter` with a Java 17 target, and `settings.gradle.kts` declares the Wire plugin it needs.

## Not done yet

| Item | Where it stands |
|---|---|
| Pod data | Wired behind `ATRIUM_REAL_DATA` and checked on the simulator with no pod (empty and not-connected states). Not yet run against a real pod. |
| Permissions | One flow: Nearby devices (never for location) on Android 12+, location permission and Location on for Android 8-11, notifications on Android 13+ and iOS, each asked at the step that explains it, with a sheet that opens the right Settings page on a refusal. Device shows a notice with the fix for Bluetooth not allowed, Bluetooth off, and battery optimisation or Background App Refresh holding back background sync. Health (HealthKit, Health Connect) permissions come with that feature. |
| Background sync | Written for both platforms (above); compiled, not yet run against a pod. Timing of iOS wake-ups and Android's job under Doze need a pod on the bench. |
| Pod software update | Built and tested against fakes (GitHub, MCUmgr, the native host). Not yet run against a real pod: the hand-over, the slot list read after the reset, upload resume after a drop, the 10 s swap wait and the reconnect timing need an nRF52 DK. |
| Oxygen trend | Shown on Sleep with Labs on after 7 earlier nights; checked on the reference night repeated. Its band ("close to your usual" within 0.03 of the baseline ratio) is [ASSUMED]. |
| Recovery, Sleep and Effort | Atrium's own formulas (`scores_v0`) from the pod's log; every constant [ASSUMED] until pod nights exist. Recovery shows the learning state for the first 5 nights. |
| Sleep stages | The 3-class model runs only with Labs on and on mid and high phones; it splits asleep epochs into REM and non-REM (shown as light), so the minutes asleep never depend on it (agreement 0.28). |
| Side tilt check | `sideFromTilt` is ported; the fit guide doesn't yet ask for the tilt or stream Motion Data 0x0204. |
| Hardware testing | Not tested against a pod, because none exists yet. Next is an nRF52 DK running the firmware. |
