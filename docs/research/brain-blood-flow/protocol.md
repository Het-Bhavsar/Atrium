# Recording protocol for the depth study

**Participants:** healthy adults, seated or lying, with a second person present at every session.

**Stop rule:** stop at once if anyone reports dizziness, tingling, chest discomfort or feeling unwell. Wait at least 2 min seated before any further step, and end the session if it recurs.

Each participant is their own control.

## Before recording

- **Ethics:** results meant for publication need written informed consent and an ethics committee review that fits the country of recording [VERIFY the route for a non-clinical wellness device]. Participants can stop at any time without giving a reason.
- **Exclude** anyone who:
  - is pregnant;
  - has known heart, lung or vascular disease;
  - has fainting episodes, migraine triggered by breathing changes, epilepsy, or Raynaud's;
  - has had head or neck surgery;
  - is under 18.
- **Pod:**
  - Full mode (green, IR and red). Start above the green battery limit.
  - Use the research firmware mode at 100–200 Hz when it exists (`data_inventory.md`).
  - Placement: no-glasses spot (temporal hollow), pod upright. Record the side.
  - The fit guide's press check must pass before recording starts.
- **References, in order of value:**
  1. capnometer (end-tidal CO₂);
  2. chest strap with ECG/RR;
  3. finger or earlobe PPG on the other side (a systemic reference);
  4. continuous blood pressure, if available.

  Transcranial Doppler needs a hospital neurology lab collaboration and a probe on the other side.
- **Sync:** three firm taps on the pod at the start and the end, logged on every device.
- **Ambient check:** 30 s with a dark cloth over the pod and the head, then 30 s uncovered in room light. Log the room lighting.

## Sequence (one session, about 40 min)

Log onset and end of every event on the pod's time base (seconds from the first PPG sample). The baseline for every challenge is the 30 s before its onset, so leave at least 30 s of quiet rest before each one.

| # | Protocol | How | Repeats | Response window (pre-registered) |
|---|---|---|---|---|
| P1 | Seated rest | 5 min, eyes open, no talking | 1 | whole period (H2) |
| P2 | Superficial temporal artery compression | Firm finger pressure just in front of the ear, above the cheekbone, on the pod's side, 15–20 s. Confirm the pulse under the finger stops | 3, 40 s apart | 5–15 s into compression |
| P3 | Hyperventilation | Fast, deep breathing, seated, 30–60 s, paced by the operator | 1, then 2 min recovery | last 20 s |
| P4 | Breath-hold | After a normal breath out, 20–30 s | 3, 60 s apart | last 10 s of the hold plus 10 s after |
| P5 | Posture | Stand to supine, 2 min; supine to stand, 2 min | 1 each | 30–90 s after the change |
| P6 | Jaw | Chewing gum, talking (reading aloud), jaw clench (5 s on, 5 s off), 60 s each | 1 each | whole period (H5) |
| P7 | Cold pack | Cold pack on the forehead and cheek, same side, **not on the pod**, 60 s | 1 | 30–60 s |
| P8 | Light exercise | Cycling or stairs, 3–5 min, then 3 min seated | 1 | descriptive only |

Notes on each step:
- **P2** is the key scalp test. If every channel collapses, the pulse is from the scalp.
- **P3:** hyperventilation lowers CO₂. Stop on tingling or dizziness.
- **P4:** a breath-hold raises CO₂. Do not hold after a deep breath in.
- **P5:** posture shows how hydrostatic and systemic changes confound everything else.
- **P7** is the second scalp test. It also triggers a mild diving response (a heart-rate drop), so it is not purely local.

Order: P1, P2, P4, P3, P7, P6, P5, P8. The CO₂ challenges come before posture and exercise, so that systemic state is near rest. Randomise P2 against P7 between participants.

## Files per session

Build one session folder per participant and session (format in `README.md`):
- the pod capture;
- the event log;
- each reference device's raw export.

Keep the raw exports unchanged next to it.

## What each challenge can and cannot show

| Challenge | Changes | What a channel's response can mean |
|---|---|---|
| P2, P7 | scalp perfusion (P7 also a little systemic) | A channel that does not fall with green samples tissue that the STA or skin cold does not reach: muscle first, at this spacing |
| P3, P4 | CO₂, plus heart rate, blood pressure, and skin and muscle vessel tone | A response beyond green is consistent with muscle, subcutis or deeper tissue. It is **never intracranial on its own**, because the systemic and muscle effects of CO₂ travel the same way |
| P5, P8 | systemic and hydrostatic | Confounders, recorded to bound them |
| P6 | the temporalis muscle and pod movement | Artefact library for daytime wear |
