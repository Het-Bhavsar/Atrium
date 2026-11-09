# Brain blood flow?

A pod on the side of your head sits little more than a centimetre from the brain. So the first question people ask me
is whether Atrium can see blood flow in the brain. I wanted that to be true, so before I promised anything I tried to
prove it. This page is what I found, written so you can check every step.

**Short answer: no.** A sensor this small reads the blood in your scalp. To reach the brain with light you need the
light source and the detector about 3 cm apart, and even then most of what you see is still scalp and muscle. Atrium
does not measure brain blood flow, and I won't claim it does.

<p align="center">
  <img src="images/research/light_depth_explainer.jpg" alt="How deep light travels at 2 mm and at 30 mm spacing" width="100%">
</p>

## 1. How a light sensor reads blood

An optical sensor shines light into the skin and catches what scatters back. Blood absorbs light, so every heartbeat,
when the small vessels swell, a little less light comes back. That rhythm is the pulse.

The light that returns does not go straight down and up. It wanders through the tissue and comes back along a curved,
banana-shaped path between the LED and the photodiode. **How deep that banana reaches depends on how far apart they
are.** Put them close together and the light stays near the surface. Move them apart and the light that makes it across
has, on average, travelled deeper; a common rule of thumb is a depth of a third to a half of the spacing.

The colour matters too. Green is absorbed quickly and stays in the top millimetre or two. Red and near-infrared go
further, which is why every brain-imaging optical system uses near-infrared.

## 2. What sits under the temporal region

From the skin inwards, at the spot where the pod sits:

| Layer | Typical thickness | Blood supply |
|---|---|---|
| Skin and scalp | about 3 mm | Superficial temporal artery |
| Temporalis muscle (the jaw muscle) | about 5 mm, from 3.7 to 17 mm between people | Deep temporal arteries |
| Skull | about 2–3 mm | |
| Cerebrospinal fluid | about 2 mm | |
| Brain | | Internal carotid and vertebral arteries |

Two things follow from this.

- **The pulse you feel at the side of your head is a scalp pulse.** The superficial temporal artery is a branch of the
  external carotid, which feeds the face and scalp, not the brain. Press just in front of your ear and you can feel it.
- **There is a muscle in the way.** The temporalis lies between the scalp and the skull. It has its own blood supply,
  it changes with chewing and clenching, and any light that goes deeper than the scalp passes through it first.

## 3. The two approaches we modelled

We simulated light travelling through these layers with a layered photon Monte Carlo (40 million photons per run,
green, red and infrared) and asked one question: what share of the pulse signal comes from each layer?

| | Atrium today | fNIRS |
|---|---|---|
| What it is | Optical heart-rate sensing (PPG): an integrated module with green, red and infrared LEDs and a photodiode in one 2.9 × 4.3 mm package | Functional near-infrared spectroscopy: near-infrared light, separate source and detector |
| Spacing | about 2 mm | about 30 mm |
| Green | more than 99.9 % scalp; brain 0 | not used |
| Red and infrared | 88–89 % scalp, 11–12 % temporalis muscle, **brain 0.006–0.008 %** | **brain about 10 %** (10.6 % in our model) |
| With the thinnest published layers | brain about 0.05 %, up to about 0.1 % with a 3.7 mm muscle | |
| Fits on the side of the head? | yes, it is the pod | needs a band or a 3 cm probe |

Our 30 mm result lands inside the 8–13 % that published head-model simulations report at 25–35 mm spacing. That
agreement is why I trust the model at 2 mm too.

<p align="center">
  <img src="images/research/depth_share_simulated.png" alt="Simulated share of the red and infrared pulse from brain and from temporalis muscle against spacing" width="80%">
  <br><sub>Share of the red and infrared pulse from the brain (solid) and from the temporalis muscle (dashed) as the spacing grows. Simulated.</sub>
</p>

**The spacing is set by the part, not by us.** Atrium's sensor is an integrated optical module (MAXM86161): the green,
red and infrared LEDs, the photodiode and the analog front end all sit in one 2.9 × 4.3 mm package. The distance from
emitter to diode is fixed inside that package at about 2 mm. No firmware, setting or algorithm can move it, so the
light can only ever sample the top few millimetres.

**We also tried a far detector.** The first design, Rev B.1, used separate LEDs and two photodiodes, one of them
18.5 mm away from the LEDs as a "far channel" to look deeper. The model showed what that buys: at about 20 mm the
brain's share is still only about 1 %, while the temporalis muscle is about three quarters of the red and infrared
pulse. It cost 8 mm of length for a signal dominated by the jaw muscle, so it was dropped
([`legacy/`](../legacy/README.md)).

## 4. Why better processing can't rescue it

The obvious reply is "the brain part is small, so filter harder". We tested that too.

- **The signal is hundreds of times too small to find.** I simulated the study I would actually run: people breathe
  extra CO₂, which widens brain vessels, and we look for a matching change in the pod's signal. To detect a brain
  share reliably (80 % of the time) we would need it to be about **3.8 % with 12 people, or 2 % with 24**. Physics gives
  0.008 % with typical layers and 0.05 % with thin ones: **40 to 450 times too small**.
- **Faster sampling does not help.** At 200 samples a second instead of 25, the shortfall barely moves (from 454 to 360
  times). A perfect, noiseless sensor is still 231 times short. The limit is the natural beat-to-beat change in scalp
  and muscle blood, which looks exactly like a deeper pulse would.
- **Muscle fools the test.** The temporalis also responds to CO₂. In simulation, a 5 % muscle response alone produced a
  false "deeper than skin" result in 48 % of 12-person studies and 76 % of 24-person studies, with no brain signal at
  all.
- **Timing differences prove nothing on their own.** Infrared arrives about 25 ms before green in a real finger
  recording (22 people, 500 samples a second, public data), and there is no brain in a finger.

So a deeper-looking number from a small sensor is far more likely to be muscle than brain.

## 5. What to ask any device that claims to see brain blood flow

These are the questions I asked myself. They are fair to ask of any product, including this one.

1. **How far apart are the light source and the detector?** Under about 2 cm, the brain's share is tiny. The physics
   does not care how good the algorithm is.
2. **Is there a short channel?** Serious fNIRS adds a second detector about 8 mm from the source that sees only the
   scalp, and subtracts it. Without one, scalp changes are reported as brain changes.
3. **What was it validated against, and in how many people?** Transcranial Doppler or MRI blood-flow imaging, in a
   published study, is the bar. A correlation with heart rate is not.
4. **Does the signal survive the controls?** If pressing the temporal artery in front of the ear wipes it out, it is
   scalp. If clenching the jaw changes it, it is muscle.
5. **Measured or simulated?** Say which. Every number on this page is simulated or from finger data, and is labelled
   that way.

## 6. What we will test next

Nothing above is a head measurement yet. These are the experiments, with the smallest number of people that can give
a clear answer:

| Test | What it shows | People |
|---|---|---|
| Press the temporal artery during recording | If every colour collapses, the pulse is scalp | 10 |
| Breathe CO₂, with a capnometer, chest ECG and finger reference | Whether anything deeper than scalp responds, with muscle controls | 24 |
| Ultrasound at the pod spot | Real layer thicknesses, so the model fits each person | 5 |
| A two-distance research probe (8 mm and 30–35 mm) with transcranial Doppler | The only route to a real brain comparison. A research tool, not the pod | 20 |

We will publish the results here, including the negative ones.

## 7. What Atrium is

A wellness device that reads your pulse, heart-rate variability, skin-temperature trend, sleep and activity from the
scalp, very well, in a pod small enough to forget. That is a lot, and I would rather do it honestly than promise
something the physics can't give.

— Het

## Sources and method

- **Photon model:** layered Monte Carlo, 4 × 10⁷ photons, medium skin; layers 3 mm scalp, 7 mm or 5 mm temporalis,
  3 mm or 2 mm skull, 2 mm CSF [ASSUMED for any one person]. Output for the earlier sensor layout is in
  [`legacy/rev-b1/hardware/simulation/results/`](../legacy/rev-b1/hardware/simulation/results/).
- **Study power:** 500 simulated studies per point, with the analysis fixed before any data was looked at.
- **Finger timing:** pulse-transit-time-ppg 1.1.0 (PhysioNet, ODbL 1.0), 22 people, sitting, 500 Hz.
- **Anatomy:** published imaging puts the adult temporalis at about 5 mm (3.7–17 mm) and the temporal skull at about
  2 mm; published head-model simulations put the brain share at 8–13 % for 25–35 mm spacing.
- **Status:** no Atrium head recording exists yet. Every result here is simulated or from finger data.
