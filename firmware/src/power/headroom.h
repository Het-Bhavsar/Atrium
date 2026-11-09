/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * LED headroom policy. The MAXM86161 LEDs run straight from VBAT, and green has
 * the highest forward voltage: up to 3.8 V, plus the driver's compliance. That
 * puts green at about 3.5 V on the 31 mA range [VERIFY Vf against current]
 * (REV_B2_COMPACT.md). The cell sags by I x R during each LED pulse, so the
 * limit applied to the VCELL reading is green-min-mv plus that sag. Below it,
 * green slots are dropped and the pulse comes from IR, or red when a mode has
 * no IR slot. Plain C, no Zephyr.
 */
#ifndef ATRIUM_HEADROOM_H_
#define ATRIUM_HEADROOM_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HEADROOM_HYST_MV 50

struct headroom {
	uint16_t limit_mv;
	bool green_ok;
};

/* VCELL limit for green: green_min_mv plus the pulse sag green_ma x cell resistance. */
uint16_t headroom_limit_mv(uint16_t green_min_mv, uint16_t green_ma, uint32_t cell_mohm);

void headroom_init(struct headroom *h, uint16_t limit_mv);

/* Feed each VCELL reading. Returns whether green may run. */
bool headroom_update(struct headroom *h, uint16_t vbat_mv);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_HEADROOM_H_ */
