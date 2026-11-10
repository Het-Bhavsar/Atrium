/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Fast over-current cut-off for the pod feed. The 100 ms state-machine loop needs three
 * readings (300 ms or more) to call a contact fault; the 1N5819WS diodes in the feed path
 * must see a short for no more than 50 ms (hardware/simulation/charging_tolerance.md, build
 * item B-8). So main.c decides this itself, every 20 ms LED frame:
 *  - it tells oc_fast every time it drives VIN_EN (oc_fast_vin), and so knows when VIN last
 *    went on: at the feed start and at each VIN-on edge of the signature and soft-reset
 *    pulses;
 *  - a frame is armed while VIN is on and at least oc_blank_ms (10 ms) have passed since
 *    that edge (oc_fast_armed). An armed frame reads the pod current once;
 *  - OC_FAST_COUNT (2) armed readings in a row over the limit: VIN off at once.
 * Frames come every 20 ms, so the first armed one is less than 20 ms after the blanking
 * ends and the cut comes on the next: within blank + 40 ms of the VIN edge for a short that
 * is there from the start (10 + 40 = 50 ms), and within 40 ms plus one reading (0.4 ms) for
 * a short that starts later. Which 100 ms tick the edge falls on does not matter.
 * The blanking covers the pod's inrush (case/README.md, "Inrush and the 10 ms blanking"): under 2 ms
 * expected, so 10 ms is a 5x margin. Inside it only the SY6280 current limit applies.
 * The state machine is told afterwards (case_in.fast_trip) and handles the fault and the
 * 30 s retry. Plain C, host-tested.
 */
#ifndef OC_FAST_H_
#define OC_FAST_H_

#include <stdbool.h>
#include <stdint.h>

#define OC_FAST_COUNT 2 /* readings in a row over the limit */

struct oc_fast {
	uint8_t over;    /* readings in a row over the limit */
	bool vin;        /* VIN_EN as main last drove it */
	uint32_t vin_t0; /* when main last drove it on */
};

/* main drove VIN_EN to `on` at now_ms (every write; only a rising edge restarts the
 * blanking). Going off clears the count. */
void oc_fast_vin(struct oc_fast *f, bool on, uint32_t now_ms);

/* VIN is on and at least blank_ms have passed since it went on. */
bool oc_fast_armed(const struct oc_fast *f, uint32_t now_ms, uint16_t blank_ms);

/* One reading. armed: from oc_fast_armed. Returns true when VIN must go off now. */
bool oc_fast_sample(struct oc_fast *f, bool armed, int32_t i_ua, bool sat, int32_t limit_ua);

#endif /* OC_FAST_H_ */
