/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "vinpat.h"

#include <string.h>

void vinpat_init(struct vinpat *v, const struct vinpat_cfg *cfg, bool vin)
{
	memset(v, 0, sizeof(*v));
	v->cfg = *cfg;
	v->vin = vin;
}

enum vinpat_ev vinpat_edge(struct vinpat *v, bool vin, uint32_t now_ms)
{
	if (vin == v->vin) {
		return VINPAT_NONE;
	}
	v->vin = vin;
	if (!vin) {
		v->off = true;
		v->off_t0 = now_ms;
		return VINPAT_NONE;
	}
	if (!v->off) {
		return VINPAT_NONE; /* VIN at power-up: nothing to time */
	}
	v->off = false;

	uint32_t d = now_ms - v->off_t0;

	if (d < v->cfg.min_off_ms) {
		return VINPAT_NONE; /* bounce */
	}
	if (d > v->cfg.max_off_ms) {
		v->n = 0;
		v->on_case = false; /* lifted, or the case let go */
		return VINPAT_NONE;
	}
	/* keep the pulses that started inside the window ending now */
	uint8_t k = 0;

	for (uint8_t i = 0; i < v->n; i++) {
		if (now_ms - v->start[i] <= v->cfg.window_ms) {
			v->start[k++] = v->start[i];
		}
	}
	if (k == 3) {
		k = 2; /* drop the oldest */
		v->start[0] = v->start[1];
		v->start[1] = v->start[2];
	}
	v->start[k++] = v->off_t0;
	v->n = k;
	if (k == 3) {
		v->n = 0;
		return VINPAT_RESET;
	}
	if (k == 2) {
		v->on_case = true;
		return VINPAT_CASE;
	}
	return VINPAT_NONE;
}

bool vinpat_on_case(const struct vinpat *v)
{
	return v->vin && v->on_case;
}
