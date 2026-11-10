# Mechanical design

The pod housing, battery bay, board clamp, skin pad, Plinth, charging case enclosure, programming jig and packaging, with the checks each one passes.

**Envelope: 20.9 × 11 × 5.3 mm** (decided 2026-11-07; see [architecture.md](architecture.md#why-209-mm)).

**Why not 20.4 mm:** the length is set by the chain along the radio-island bottom (inner edge, then the flex tongue, the window and the antenna zone), not by the crystal.
- With the tongue and a 1.6 × 1.2 crystal at 20.4 mm, every part places, but the antenna would start 0.56 mm before its zone, over the MAXM86161.
- Two single-row flex tails beside the window land on the ±60° / ±86° clamp bands and leave 5 parts unplaced at 20.4 mm, and 15 at 19.9 mm.

## Files

| What | Where |
|---|---|
| Pod shell, base, gasket, skin pad (STEP, STL, GLB) | `mechanical/pod/` |
| Skin pad die-line | `mechanical/pod/DXF/skin_pad_dieline.dxf` (the older `skin_adhesive_dieline.dxf` is the superseded cradle adhesive) |
| Plinth | `mechanical/plinth/` |
| Charging case enclosure | `mechanical/case/` |
| Programming jig | `mechanical/programming-jig/` |
| Retail packaging | `mechanical/packaging/` |
| Renders | `mechanical/renders/` |
| Interference and fit reports | `mechanical/reports/interference_report.txt`, `mechanical/reports/case_check_report.txt` (re-run 2026-11-10 on the released boards: the islands after the spacing change, O-45, and the case board with the BQ24040 and TH2) |
| Pod board outlines (DXF) | `hardware/design-data/` |
| Case PCB outline | `mechanical/case/DXF/case_pcb_outline.dxf` |

## Checks at 20.9 mm

Every check below passes on the current CAD.

| Check | Result |
|---|---|
| Envelope | 20.90 × 11.00 × 5.30 |
| Z stack | 0.5 top + 0.2 swell + 4.0 cell + 0.1 stiffener + 0.12 flex + 0.03 bond + 0.35 base = 5.30 |
| Interference: housing, islands, all 61 placed parts, cell, stiffener, flex | None |
| Islands clamped: posts under, ribs over, 6 points | Pass |
| Clamp bands clear of part courtyards (61 parts as placed) | Pass |
| Shell wall, capsule with the R 1.8 round | 0.38 mm (min 0.30) |
| Module corners inside the window | ≥ 0.05 (window 3.4 × 6.14) |
| No module metal in the antenna zone | Module to x 18.56, zone from 18.76 (the window and gasket reach 18.81; no metal) |
| Antenna at the tip (ANT1 x 18.935–19.865) | Body 0.175 inside its zone, pad corner 0.20 from the board edge, 0.30 to the shell wall, 2.7 to the nearest rib |
| Contact holes from the window | 1.96 mm (rule ≥ 1.5) |
| Skin NTC (TH1) boss gap | 0.10 mm, gel-filled |
| Plinth magnets from the antenna zone | 7.06 mm (rule ≥ 5) |
| Stiffener over the magnets, either way round | ≥ 65 % of each magnet face |
| Pod rests on its frame in the Plinth, module clear | Base on the nest floor, module 0.20 clear |
| Skin pad inside the footprint, full pad not a ring | 18.8 × 8.9, 1.05 inside the outline; 130 mm² bonded, largest hole 13.3 % |
| Pad in the bed, behind the frame; module level with the pad | Edge 0.25 inside the lip; pad face and module both 0.10 below the frame |
| Base floor under the bed | 0.30 mm |
| Pad lands in the bed off the card | 0.20 worst case against 0.25 clearance |

The fit report also records the flex bend radii, the worst-case cell and the mass.

## Form: the capsule

**Shape:**
- A stadium capsule: semicircular ends and straight long sides, 20.9 × 11 × 5.3 mm. This is the smallest size the parts allow: the 9 × 9 cell, the islands and the antenna zone set it.
- The top edge rolls into the side in one large R 1.8 round, the largest the parts allow:
  - R 2.0 leaves a 0.29 mm wall;
  - a larger inner round hits the cell.
- Minimum wall 0.38 mm, with the cavity's top edge rounded R 1.0.
- The round costs about 11 mm³ of inside space; it is clear of the antenna and both islands.
- The top is flat.

**Look:** bead-blasted satin silver-grey (#A9ADB2) with a metallic sheen, even, with a soft highlight along the round. No polish band, no lights, no logo, no visible seams. The base is uncoated, in a matching plain grey (#A3A6AA).

**It must look metallic without being metal.** The KH-1608 antenna sits 0.30 mm from the shell wall, and the shell is the antenna's window.
- **Rejected: a metal shell** (bead-blasted or anodised aluminium or titanium). A conductive wall 0.3 mm from a chip antenna shields and detunes it; it would need a non-metal end cap over x ≥ 18.76 and a new antenna study.
- **Rejected: PVD on zirconia or plastic** (TiN, CrN, Ti). PVD is a continuous conductive film, so it acts like a metal shell, and masking the antenna end would leave a visible band.
- **Rejected: aluminium-flake metallic paint.** The flakes can couple and add loss; avoid it on the antenna end at least.
- **Chosen: NCVM** (non-conductive vacuum metallisation). A metallic film of separate tin or indium islands looks like metal and passes RF, under a matte UV hardcoat. It goes on a moulded PC shell with a bead-blast-textured mould (VDI 3400 ref. 24–27), so the satin comes from the texture and not from the coating.
- **Skin:** the base is not coated. The shell rim, which is part of the frame, carries the hardcoat: either mask the rim during NCVM or test the hardcoat to ISO 10993-5/-10.
- **RF check:** on the EVT board, measure return loss and efficiency with a bare shell and a coated shell on the VNA. Re-tune the 3.3 nH / 1.0 pF match if the coated shell shifts it [VERIFY].
- **Weight:** a PC shell is 0.25 g, against 1.27 g in zirconia.

**Getting the look on early pieces:**
1. **SLA print, painted (1–10 pieces).**
   - Print at 25 µm layers, with supports only inside the cavity.
   - Sand the top and round in one smooth pass (400 → 800 → 1500 grit) so the round flows into the flat without a ridge.
   - Fine bead-blast (or 1500 grit) for the satin, filler primer, then a **silver-grey mica or pearl metallic** base coat (non-conductive; no aluminium flake), then a matte 2K clear. An NCVM shop can also coat SLA parts.
2. **Vacuum casting (10–30 pieces).**
   - Finish the SLA master to satin with a fine bead-blast or 1500 grit; the silicone mould copies the satin into every casting.
   - Cast a grey PU, then paint as in step 1 (mica metallic, matte clear), or send to an NCVM shop. Put the parting line at the skin edge.
   - 0.38 mm walls are thin for vacuum casting: ask for pressure casting and a low-viscosity resin, or accept R 1.5 (0.50 mm walls) for cast pieces.
3. **Production:** injection-moulded PC with a textured mould, NCVM silver-grey and a matte UV hardcoat. Zirconia with NCVM stays an option for a premium edition, but test its detuning on a coupon first (εr ≈ 30).

## Pod parts

| Part | Production | Builder version | Notes |
|---|---|---|---|
| Top shell | Injection-moulded PC, bead-blast-textured mould, NCVM satin silver-grey, matte UV hardcoat | SLA resin, mica silver paint, matte clear | Capsule: one continuous side wall, flat top, R 1.8 round, 0.38 mm min wall. Ribs inside hold the islands (below). The coating is non-conductive; measure the antenna with bare and coated shells (O-36). |
| Base plate | PC or PA12, uncoated plain grey #A3A6AA | SLA, same resin | **0.35 mm** (was 0.45; the 0.1 went to the stiffener). Sits inside the shell wall (no visible step). Skin side: a 0.25 mm lip (the frame, with the shell rim) round a 0.05 mm pad bed, leaving a 0.30 floor. It carries the pill window, 5 contact holes, 7 clamp posts and the TH1 boss. |
| Window gasket | Black LSR silicone | Black TPU 85A or cut black silicone sheet | Fills the 3.4 × 6.14 window around the MAXM86161, flush with the bed floor; ≥ 0.25 mm ring at the sides, 0.05 at the corners |
| Contact stiffener | 0.1 mm SUS430 (ferritic), on the flex top under the cell | Same | Spreads the pogo load over the pouch, and is the Plinth magnets' target. JLC FPC steel stiffener [VERIFY options] |
| Base bond | Medical UV-cure adhesive [VERIFY grade] | Same, or a thin bead of skin-safe epoxy | Seals the full perimeter; this is the sweat seal |

Skin-contact materials need ISO 10993-5/-10 data: base, gasket, the shell rim's hardcoat (or mask the rim), resin and pad adhesive.

## Board clamp

Each island is held at its arc edge between base posts (under) and shell ribs (over) on a bare band 0.35 mm deep and 1.0 mm long: the power island at 114° and 246°, the radio island at ±60° and ±86° about each end centre. The power island's third support is its flex landing (10 soldered pads at the inner edge); a tip point at 180° would take the only spot the IMU fits. Without the clamp nothing holds the islands in Z: there was 2.15 mm of air above the radio island, and nothing pressed the MAXM86161 onto the skin.

Assembly: the islands, flex and cell drop into the inverted shell onto the ribs, then the base goes in and its posts clamp them.

**Board thickness (decided 2026-11-08):** both islands are 6-layer HDI, JLC06081H-2313, 0.82 mm finished (±0.10), Tg170. They were 4-layer 0.6 mm. The board grows upward: its bottom face stays at z 4.00 on the base posts, so the MAXM86161 stays 0.1 mm proud and the TH1 boss and flex lands don't move. The shell ribs reach 0.22 mm further down. All checks pass at 0.82 and at 0.92 mm: the tallest top parts keep 1.68 mm (L1) and 1.75 mm (U11) under the roof, and the closest top body to the shell is U11 at 0.14 mm, sideways at the power tip and independent of the thickness. The ±0.10 thickness is taken by the clamp, not by the parts: see O-38.

**Routed boards (2026-11-08):** the assemblies use the STEP exports of the routed boards (power and radio PCBA with every part model, tracks and vias, and the flat flex), and every part body is checked against the shell, base, gasket, cell, stiffener and flex. The exports are in the top-view frame (not mirrored), with z = 0 at each board's bottom face; each island sits bottom face on the posts. The flat flex is bent in the CAD: its bottom is on the base bond, the 0.0335 mm left under the stiffener is its PSA, and two tangent arcs run from each stiffener end up to the land pads. What the real parts showed:
- The U3 (MAXM86161) 3D model is a 3.11 × 4.53 mm placeholder box centred 0.08 / 0.09 mm off its pad field. It runs 0.30 mm into the gasket and the base window edge. The footprint is centred (pads at 17.11, 5.50), so only the model needs fixing: 2.90 × 4.30 × 1.40. Until it is fixed, the assemblies and clash checks use the datasheet body.
- The exported board stack is 0.844 mm (0.82 + mask), so the six rib tips crush it by 0.024 mm (O-38). The clamp bands now have solder-mask openings on both sides; the board STEP still exports one 0.844 mm slab, so the CAD check keeps reporting 0.024 until a measured board says otherwise.
- The flex fab outline is the flat pattern: each S-bend needs more flex than the straight run between the stiffener end and the lands gives, 0.47 mm more on the power end (0.85 mm run) and 0.51 mm on the radio end (0.76 mm run). The outline steps from tongue to body at the stiffener edges (x 4.75 / 13.30), so no outline corner lies in a bend. The fit check passes on the flat-pattern export (O-39 closed).
- The bends clear the cell overhang by 36 µm (power) and 45 µm (radio). If the flex top sat against the stiffener instead, it would be 12 / 20 µm. r 0.425 / 0.382 mm = 4.9 / 4.4 × the 0.0865 mm flex (O-25).
- These bottom parts sit inside the 0.1 mm courtyard of the flex outline, which the PCB DRC can't see because the flex is a separate board: C53 0.023, C55 0.043, R51 0.062, D10 0.067, U2 0.068, C21 0.093. C30 is 0.023 from the radio −86° rib.
- TH1's model is 0.24 mm tall, not the 0.33 mm in the footprint library, so the gel gap is 0.19 mm. Raise the boss 0.09 mm once the real part height is confirmed.
- The radio board has square shoulders 0.23 mm outside the edge cuts (x 13.87–15.05, both sides) and is still 0.113 mm from the side wall.
- Passing: all five contacts are centred in their holes (0.000 offset), all 20 jumper pads are aligned, the MAXM86161 is 0.100 proud, ANT1 is 0.455 from the wall, and the tallest bottom part is U10 at 0.61 (U11 is the tallest on top, at 0.92).

**PCB keep-out:** the clamp bands are free of parts on both sides. Placement keeps them clear with the same sector shape the CAD uses for the ribs and posts. The current placement (re-placed around the hand-placed RF chain) has 61 parts, with TH4 on the radio top and TH1 at (15.15, 9.55) on the radio bottom; the base boss follows TH1. No band is in the antenna zone.

## Battery bay (Sharvi HT19 400909, 4.0 × 9 × 9 mm)

- Pocket 9.3 × 9.3 mm. Cell top at z 0.7 below the top face, leaving 0.2 mm of air for swell.
- **The bay is sized for the nominal cell.** A worst-case cell (9.5 × 9.5 × 4.2 + 8 % swell) needs a 9.55 mm pocket and H ≈ 5.64. Measure 10 cells before freezing the shell.
- Tabs exit toward the power island and fold down onto its CELL+ / CELL− pads. Nothing rigid presses on the pouch: the stiffener and flex lie under it.
- **Flex S-bends:** the rise from the flex body to the island bottom is 0.80 mm (0.1 more than before, because of the stiffener).
  - The bend can start at the stiffener's end, not the cell's: past it the cell sits 0.1 mm above the flex, and the first arc stays under that.
  - The run from the stiffener end to the first pad column is 0.85 mm (power) and 0.76 mm (radio). Two tangent arcs then give r ≈ 0.43 / 0.38 mm. The power end of the stiffener moved in to x 4.75 (2026-11-08) when the power landing column moved to x 3.675.
  - With single-copper-layer bend zones (about 0.07 mm), that is about 5.4 × t: borderline for a static bend [VERIFY JLC's minimum]. On the full 0.12 mm two-layer build it would be 3.2 × t, so the bend zones must be single-layer.
  - Keep the stiffener ends at x 4.75 / 13.30: they set the bend start. The radio end can't move in: that bend runs under the cell overhang, and at 13.25 it would clear the cell by about 5 µm.
  - Every contact pad stays on the stiffener: GND_B's pad edge is 0.15 mm inside x 13.30 at the 2.2 mm GND pitch (it was past the edge at 2.4).

## Skin pad

The adhesive must not show.

**Frame and bed:**
- The skin face is a **frame** and a **bed**. The frame is the shell's 0.5 mm rim plus a 0.25 mm lip of the base, flat at the skin plane. The pod rests on it in the Plinth, and a pillow or glasses arm meets it at the skin line.
- Inside the frame the base is recessed **0.05 mm** into a stadium bed, which leaves a 0.30 mm floor and the full 0.35 mm at the lip.
- The pad sits in the bed with **0.25 mm clearance** all round. Its skin face stands 0.10 mm below the frame, level with the optical module, so its edge is tucked into a 0.1 mm slot behind the frame where nothing can catch it. Nothing shows from the front or the side.
- **Location:** the bed's own edge locates the pad, and the module's 0.1 mm boss inside the pad's hole stops it sliding. The hole is a pill that frames the base window (+0.1 mm), so the skin face shows only the frame, the pad and the pill window, with the black gasket between pad and module. There are no ribs or grooves on the pod.

**The pad** (die-line `mechanical/pod/DXF/skin_pad_dieline.dxf`):
- A disposable double-coated medical pad, 18.8 × 8.9 mm, 1.05 mm inside the pod's outline.
- **0.15 mm thick:** the bed depth (0.05) plus the module's 0.10 proud [VERIFY a tape of 0.15 mm ± 0.02; the bed depth follows the tape].
- **Opaque and skin-tone:** a clear pad would pipe LED light sideways into the photodiode.
- **One hole,** a pill 3.6 × 6.34 mm round the window: 130 mm² bonded, largest hole 13.3 %. It is a full pad, not a ring.
- **No pull tab.** To take it off, tilt the pod off from its bottom end with a fingertip; the pad comes with the pod, then peels off it.
- **Off before charging:** the skin side would stick in the Plinth nest. The pad covers all five contacts while worn.
- **Supplied six to a card:** the card is the skin-side liner, under a 0.6 mm die-cut frame with a pod-shaped window round each pad (0.10 mm clearance). Press the pod into a window and the pad lands in the bed. The worst case is 0.10 frame + 0.10 die-cut registration = 0.20, inside the bed's 0.25 [VERIFY the converter's registration].

**Changing the pad** (the app shows the same steps):
1. **Take the pod off.** Tip it off from its bottom end with a fingertip, low and slow. The pad comes with it; if it stays on the skin, peel it from the bottom edge.
2. **Clean the base.** Lift the old pad's end with a fingernail (it sits 0.25 mm inside the frame, so a nail gets under it) and peel it off along the pod. Wipe the bed and the optical window with an alcohol wipe and let it dry. Never reuse a pad.
   - **Residue:** if any stays, use a skin-safe silicone adhesive-remover wipe, then an alcohol wipe to take off the oily film. Any film left on the window weakens the signal.
   - **Never** use acetone, nail-polish remover or a blade: acetone attacks the PC shell and its coating, and a blade scratches the window.
3. **Charge with no pad on.** A pad's sticky side would grip the Plinth nest.
4. **Prepare the skin.** Wipe the spot with an alcohol wipe and let it dry fully: no lotion, no sweat, no hair in the area.
5. **Pick up a new pad.** Press the pod, window end first, into one of the pod-shaped windows on the pad card and hold it 3 s. Lift it out: the pad comes with it, aligned.
6. **Place it** as the fit guide says ([placement.md](placement.md#fit-guide)), and press firmly for 10 s. Avoid a hard workout, a shower or a swim for about 30 minutes while the adhesive builds its bond [VERIFY on the chosen tape's data sheet].
7. **Change it** at every charge, when an edge lifts, or after 2 days, whichever comes first. Switch sides at each change to rest the skin. Stop if the skin stays red or itchy.

**Trade-offs against a pad with a skirt:**
- About 48 % less bonded area than the earlier cradle's ring (251 mm²), and 21 % less than the earlier flush pad (164 mm²); the frame buys back edge protection.
- No compliant skirt to resist peel at the edge. The load is not the problem (3 g at 10 g of acceleration is about 0.3 N, or 2 kPa over the pad); peel at the rigid edge, sweat and side knocks (pillow, glasses arm, hair) are.
- Expect about 1–2 days of wear per pad, against 2–4 with a skirt [EST, wear test]. The battery's roughly 4 days means a new pad at least every charge anyway.
- **Long-wear option, if the wear test fails:** a 1.0–1.5 mm skirt of clear, matte 25 µm PU film, nearly invisible on skin. Only the skirt may be clear, never the part around the window.

**Materials to test:**
- **Pod side:** medical acrylic.
- **Skin side:** a high-tack medical acrylic for all-day and sport wear (IBOA-free, ISO 10993-5/-10), against a silicone for sensitive skin. A silicone skin side is gentler but weaker in sweat, so test both through running, showers and sleep.
- **Candidates:** 3M / Solventum 1577 (double-coated PET, die-cuttable) and 2477P (silicone / acrylic) [VERIFY a total near 0.15 mm on each data sheet; the bed depth follows the tape].

**Early pieces, before a converter:**
- **Tape:** skin-safe double-sided medical acrylic tape, sold in pre-cut strips and rolls.
- **Cutting:** kiss-cut the pad from the die-line on a desktop vinyl cutter (convert the DXF to SVG). Alternatives:
  - a low-power CO₂ laser through the top liner;
  - a steel-rule die from a packaging die maker, used with a hand press.
- **Window hole:** let the cutter do it, or punch a Ø3.6 hollow punch at each end and cut the straight sides between.
- **Sport and shower tests:** put a clear PU film dressing over the pod and pad.
- **For production:** ask the tape maker for its authorised medical converters, and ask any converter for ISO 13485 and skin-contact data.

**Superseded: the adhesive cradle.** Until 2026-11-07 the skin attachment was a pill collar (0.6 mm wall, 2.0 mm tall, 0.05 mm interference per side; LSR around 50 Shore A, or TPU 85A for builders) with a 0.3 mm flange 4 mm beyond the pod, and a 2477P adhesive die-cut to the flange with a pull tab. It was replaced by the hidden pad because the adhesive and collar showed around the pod.

## Plinth

Desk charger. CAD in `mechanical/plinth/`.

- Ø28 × 8 mm puck. The pod nest is the outline + 0.15, 1.6 mm deep, so the pod sits either way round. A 0.3 mm window relief, both ways round, keeps the pod resting on its base and not on the optical module, which stands 0.1 mm proud.
- **Pins:** two pogos, VIN at the nest centre and GND at GND_B, +2.2 mm (Ø2 mm SMD class [VERIFY stroke vs the pad recess]).
- **Retention:** two Ø2.5 × 1.5 N52 discs under a 0.3 mm skin of the nest floor, ±2.55 mm across the pod from the centre pin.
  - They pull on the SUS430 stiffener 0.8 mm away. With the magnets symmetric about the pod centre, a 180° turn keeps them under steel.
  - There is no magnet in the pod; the antenna zone is about 7 mm away and the IMU has no magnetometer.
  - Pull on a 0.1 mm sheet saturates early: estimate 0.3–0.6 N total [EST, measure]. That must beat both pogo forces, so use pins of ≤ 0.3 N at working stroke. If the bench says no, fall back to a flip-down lid.
- **Light line:** a 0.8 mm ring groove for a light-pipe ring, the only light in the system.
- **Other:** the USB-C cut-out is in the side. The Plinth electronics are not designed yet.

## Charging case enclosure

The case that ships in the box (electronics in [charging-case.md](charging-case.md)). CAD in `mechanical/case/`. It is 36 × 25 × 16.75 mm, plus a 0.9 mm hinge barrel behind. The outline is a rounded rectangle (R 5) with vertical walls, a flat lid and an R 1.5 top round.
- **Stack (z, mm):** tub floor 1.0, foam 0.5, then the KP-502030 cell (5.0 reserved; the listed 4.0 makes the case 15.75 tall). Above the cell: 0.2 air, a 0.8 mm PCB, parts ≤ 1.0 under the nest floor, then the 1.0 floor. The pod sits on the floor at z 9.7. The lid pad is 1.0 thick and squeezed 0.25 by the pod's flat top, and the lid top is 1.0. The parting line is at 12.0, so the pod stands 3 mm proud of the body and is easy to lift out.
- **Parts:**
  - tub: walls, cell bay, PCB ledges, openings, hinge knuckles;
  - deck: nest, floor, magnet bosses, pogo collars, PCB clamp posts; snaps into the tub;
  - lid: barrel-hinge knuckle on a Ø0.8 steel pin, front detent onto the tub lip, magnet boss, foam pad, four pill vents beside the pod (44 mm²);
  - light pipe and button cap.
- **Orientation:** the pod lies along the depth with its window end to the back hinge. The hinge axis sits just behind the back face, so the lid swings away from the body; it is checked to 110° clear of the tub, deck and pod.
- **Nest key:** the pod outline is 180°-symmetric, and turning it maps VIN onto VIN and GND onto GND. So the key is a rigid 0.25 mm collar round each pogo, sitting in the contact holes. Turned round, the SWD collars land on solid base, so the pod sits 0.25 high and rocks. It still charges, but SWD can't reach. The module relief is at the hinge end only.
- **Contacts and retention:**
  - Four SMD pogos (free 3.25, stroke 1.0) sit at the jig pin positions and reach the pads at 0.68 compression (68 %).
  - Two Ø2.5 × 1.5 N52 discs sit 0.8 under the stiffener, as in the Plinth. With four pins (1.2 N) they can't hold the pod, so the lid pad does: about 1.5–2.7 N at 25 % squeeze [VERIFY foam].
  - A Ø4 × 2 N52 magnet in the lid gives about 18 mT at the HAL248 [VERIFY threshold].
- **PCB outline:** `mechanical/case/DXF/case_pcb_outline.dxf` (with a PNG preview) has the board edge, top height zones (1.0 under the nest, 2.8 elsewhere, 3.3 under the USB-C hump) and no parts on the bottom over the cell. Keep-outs: the magnet bosses and the six clamp pads. It also places the pogos, the USB-C (top-mount, mouth at x 17.95), the LED, the side-push switch and the HAL248.
- **Button and USB-C:** the switch is an Alps SKSCLAE010 side-push (C130445; 3.5 × 2.9 × 1.25, 2.2 N, 0.2 travel) at (9.00, −9.05), stem to −Y, stem tip 0.15 inside the wall face. The cap is a plunger through a 0.4 outer lip (face recessed 0.2) with a shoulder in the wall pocket, fitted from inside before the deck. A flat Ø1.2 key presses the stem centre, as the switch maker asks, with 0.05 pre-travel. A flexure tab cut in the wall was studied and not taken. Stem centre height 0.6 [VERIFY on the switch drawing]. USB-C C2765186 sits at (13.0, 0) rotated 90°, mouth at x 17.99 (0.01 inside the outer face).
- **Routed case PCB (2026-11-08):** 2-layer 0.80 mm, all parts on top, sitting on the ledges at z 6.7, 0.20 over the cell (J1's legs 0.10). Every part body is checked against the case: every part under the nest is ≤ 1.0 and nothing touches the tub, deck, lid, cap, light pipe or pogos. Tallest: J1 3.26 (0.24 under the deck hump), U5/U6 1.65, D1/D2 1.45, C4 1.30, all ≤ 2.8. SW1's stem tip is at y −11.165 with 0.035 pre-travel to the cap key; D3 has 0.41 air under the light pipe. The USB-C opening is a 9.4 × 3.6 R 1.0 rounded rectangle: the real shell's corners came within 0.02 of a stadium opening, now 0.22. D1/D2 are B5819WS SOD-323 at (8.2, −4.45) / (8.2, −6.25), 0.95 tall, with no body overlaps: D1–D2 0.50, D1–R3/R4 0.57, D2–SW1 0.72, D1–J1 1.12 (two SOD-123 bodies on a 1.3 pitch overlapped by 0.4 and ran 0.385 into SW1). The STEP carries no copper, so the pogo pads aren't checked from it.
- **Cell leads:** the KP-502030's bare red / black wires leave the cell's −X end at mid-thickness, run out past the cell end and rise through a Ø0.8 plated hole each, soldered both sides. The holes must sit outboard of the cell: over it there is only 0.2 mm of air for the wire, its bend and the roughly 0.3 bottom fillet. Ø1.3 pads at (−15.5, ±2.0), 0.35 from the cell end and 0.34 from the board edge; the top fillet ends 2.5 under the deck. The board has them there (one-contour outline, holes drilled as PTH) and the fit check passes [VERIFY lead exit position, wire gauge and PCM on a delivered cell].
- **Full assembly check:** the routed board with a pod seated and the lid closed shows no interference.

## Wear orientation

The pod is worn **straight upright**: long axis vertical, in front of the ear, at about eyebrow-to-eye level, with no tilt (decided 2026-11-07; an earlier layout tilted it 10° toward the eye).
- **Radio end up.** The antenna and the optical window are both at the radio end (window centre 3.8 mm from that tip), so both sit up. The antenna then faces open air, away from the jaw and from the glasses arm and hinge (worst if metal). The window sits in front of the ear, over the region of the superficial temporal artery; placement is confirmed on people in EVT.
- **Glasses arm.** It must not cross the pod (5.3 mm tall). With glasses the pod moves up to eyebrow level (position A in [placement.md](placement.md)), so the arm passes under its bottom end: 4.2 mm clear in the placement figure, 0.6 mm in the worn render (`hero_worn.png`). Nothing of the pad shows outside the pod, so there is no flange for the arm to catch.
- **Taking it off:** tip the pod off from its bottom end (the power end); there is no pull tab.
- **Pod axes when worn:**
  - +x = up (antenna end);
  - +z = out of the head (top face);
  - +y = z × x, which points backward on the right side of the head and forward on the left.
  - Firmware maps the IMU chip's axes onto these from U11's placement; ML and firmware are tested at ±15° of placement error.

## Programming jig

See [flashing-and-test.md](flashing-and-test.md). A 44 × 30 × 12 mm block with a nest, 4 probe holes (VIN, GND, SWDIO, SWDCLK), an arrow for the window end and a cable channel. CAD in `mechanical/programming-jig/`.

## Retail packaging

CAD in `mechanical/packaging/`.

**Construction:**
- **Box:** a rigid lid-and-base box in 1.8 mm greyboard plus paper wrap (2.0 mm walls), with 1 mm corners.
  - Base: 80 × 80 × 30 mm.
  - Lid: a full telescope, 84.6 × 84.6 × 32.3 mm, 0.3 mm clear of the base.
- **Upper layer:** a moulded-pulp tray (1.2 mm wall) on top of the card insert.
  - It holds the Plinth in a pocket 6 mm deep, with the pod docked in its nest, at the centre.
  - Two thumb notches lift it out.
- **Lower layer:** a folded-card insert (0.6 mm, 12 mm deep, one divider).
  - Cable bay: the USB-C cable coiled to Ø39 × 5 mm with a paper tie.
  - Other bay: a card of six skin pads under its alignment frame.

**Checks:** no interference anywhere. The highest content, the pod top, is at 27.7 mm, under both the 30 mm base rim and the lid's inner top.

**Exports:**
- `packaging_closed.glb` and `packaging_open.glb` (lid lifted 45 mm) in `mechanical/packaging/GLB/`. Every part is its own node, and materials are named (paper_wrap, pulp, card, satin_silver_ncvm, pod_base_grey, gasket, plinth, light_pipe, adhesive, cable, plug_metal, tie).
- STEP for the box and both inserts, for the packaging supplier.
- No print or marks in the CAD.

## Renders

In `mechanical/renders/` (capsule, satin silver-grey, straight upright, skin pad in the bed): `hero_pod_three_quarter`, `hero_pod_top`, `hero_pod_low`, `hero_pod_skin_side`, `hero_pod_clear_three_quarter`, `hero_on_pad_skin_side`, `hero_skin_frame_low`, `hero_on_plinth`, `hero_worn` and `hero_worn_face_on` (next to a glasses arm; the head is an illustrative 110 mm sphere), plus `packaging_closed` and `packaging_open_xray`.

GLBs for product renders are in the `GLB/` folder of each part (pod, clear pod, pod on its pad, pod on Plinth, packaging closed and open).
