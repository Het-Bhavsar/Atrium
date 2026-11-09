/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Fast over-current cut-off for the pod feed. The 100 ms state-machine loop needs three
 * readings (300 ms or more) to call a contact fault; the 1N5819WS diodes in the feed path
 * must see a short for no more than 50 ms after the 80 mA trip (hardware/simulation/
 * charging_tolerance.md, build item B-8). main.c therefore samples the pod current once
 * per 20 ms LED frame while the feed is armed and cuts VIN itself after OC_FAST_COUNT
 * readings in a row over the limit: at most two frames, about 40 ms plus one conversion.
 * The state machine is told afterwards (case_in.fast_trip) and handles the retry.
 * Plain C, host-tested.
 */
#ifndef OC_FAST_H_
#define OC_FAST_H_

#include <stdbool.h>
#include <stdint.h>

#define OC_FAST_COUNT 2 /* readings in a row over the limit */

struct oc_fast {
	uint8_t over; /* readings in a row over the limit */
};

/* One reading. armed: the feed is on, past the inrush blanking and outside the soft-reset
 * pulses. Returns true when VIN must go off now. */
bool oc_fast_sample(struct oc_fast *f, bool armed, int32_t i_ua, bool sat, int32_t limit_ua);

#endif /* OC_FAST_H_ */
