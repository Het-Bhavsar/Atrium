# The Atrium story

How a wearable that reads the skin with light went from a first board to an EVT build in a week, and the decisions
that shaped it. The superseded designs themselves are in [`legacy/`](../legacy/).

## The idea

A pod small and quiet enough to forget you are wearing it, on the temporal region where the skin is thin and the head
moves less than the wrist. It should read pulse, heart-rate variability, skin temperature, sleep and motion; keep its
own log; and share everything about how it is made. Wellness and research only, never a medical claim.

The name: in a Roman house the atrium is the open hall at the centre, with the sky for a roof. In the heart it is the
chamber where blood arrives before every beat. Atrium reads the pulse with light, and it is open.

## Rev B.1: everything on one board

The first full design put an nRF52832, a discrete optical front end with two photodiodes (one 18.5 mm away as a deep
channel), a skin-temperature sensor and a fuel gauge on a 28 × 14 mm four-layer board, in a 30 × 16 × 7 mm shell. It
had a pocket case with USB-C. Simulation of the whole system came first: a photon Monte Carlo for the optics, a signal
budget, thermal and power models, and SPICE on the case charger. That work found an I²C address clash and showed the
deep channel added little for the 8 mm it cost.

It was too big for the head.

## Rev B.2: one optical module, many forms

The discrete optics became one module, the MAXM86161 (LEDs, photodiode and front end in 2.9 × 4.3 mm). The cell was
capped at 30 mAh. Then came the forms: tiles, wedges, slates, a sail, a blade, a disc, a pill. Each was drawn around
the real parts and checked against the cells actually in stock. None of the single-board layouts closed at the sizes
the forms wanted.

## EVT: two islands and a flex

The EVT baseline split the electronics. A 9 × 9 × 4 mm, 30 mAh cell sits at the centre; a radio island and a power
island sit in the rounded ends; a two-layer flex runs under the cell to join them and to carry the charging contacts.
19.4 mm was tried first and did not fit; 20.9 × 11 × 5.3 mm does, with every part placed on real pads and clearances.

The islands are six-layer HDI boards, 0.82 mm thick, because four layers could not escape the nRF52832's 0.4 mm ball
grid. They were routed by hand, RF first: a 50 Ω coplanar feed with arc bends to a chip antenna in a zone kept free of
copper on every layer, Nordic's reference layout around the radio, solid ground planes and stitching, and EMC rules
written down before the first track. The flex keeps single-layer bend zones and a steel stiffener under the contacts.

The enclosure followed: a satin silver-grey capsule with a flat top, a base that holds a hidden 0.15 mm skin pad, a
Plinth that charges it on magnets either way round, and a pocket case with its own small MCU. Then the production
files: Gerbers checked layer by layer, stencil apertures checked per pad, and a 2 × 3 panel that carries both islands
so a first build is one order.

## Software

The pod firmware runs on Zephyr with unit tests for every signal block. The case firmware runs on a CH32V003. The
phone app, in Flutter for Android and iOS, works without an account or a server: the data stays on the phone.

## What is next

First units, then measurements: signal quality on real heads, battery life in each mode, comfort over a night and a
day. The [roadmap](roadmap.md) lists the steps, and [open-items.md](open-items.md) the engineering questions still open.
