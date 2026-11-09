/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "headroom.h"

uint16_t headroom_limit_mv(uint16_t green_min_mv, uint16_t green_ma, uint32_t cell_mohm)
{
	uint32_t v = green_min_mv + (uint32_t)green_ma * cell_mohm / 1000U;

	return v > UINT16_MAX ? UINT16_MAX : (uint16_t)v;
}

void headroom_init(struct headroom *h, uint16_t limit_mv)
{
	h->limit_mv = limit_mv;
	h->green_ok = true;
}

bool headroom_update(struct headroom *h, uint16_t vbat_mv)
{
	if (h->green_ok && vbat_mv < h->limit_mv) {
		h->green_ok = false;
	} else if (!h->green_ok && vbat_mv >= h->limit_mv + HEADROOM_HYST_MV) {
		h->green_ok = true;
	}
	return h->green_ok;
}
