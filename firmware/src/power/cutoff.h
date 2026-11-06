/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Low-battery cut-off. Below 3.30 V loaded, off the dock, the pod enters ship
 * mode (SHIP high >= 100 ms into LPB1010 CTL). The cut-off sits above the
 * LPB1010 over-discharge trip (VDL max 2.90 V) and leaves 3.26 V under the LED
 * burst on an aged cell, above the 3.10 V the green LED needs
 * (atrium-rev-b1 SCHEMATIC_SIM.md 4 and 5.4, logic_sim.py CUTOFF).
 *
 * VBAT comes from VBAT_SENSE (VBAT / 2 on AIN5), so it is the loaded cell voltage. A
 * reading can land inside an LED burst (26 to 44 mV droop), so the cut-off
 * needs several readings in a row below the limit. Plain C, no Zephyr.
 */
#ifndef ATRIUM_CUTOFF_H_
#define ATRIUM_CUTOFF_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cutoff_cfg {
	uint16_t cutoff_mv;    /* loaded VBAT that starts ship mode */
	uint16_t low_mv;       /* below: low battery, modes drop to lite */
	uint16_t low_clear_mv; /* above: low battery clears */
	uint16_t min_valid_mv; /* readings outside min..max are ignored */
	uint16_t max_valid_mv;
	uint8_t confirm;       /* readings in a row below cutoff_mv */
};

#define CUTOFF_CFG_DEFAULT                                                        \
	{                                                                         \
		.cutoff_mv = 3300, .low_mv = 3450, .low_clear_mv = 3550,           \
		.min_valid_mv = 2500, .max_valid_mv = 4500, .confirm = 3,          \
	}

enum batt_state {
	BATT_OK = 0,
	BATT_LOW,
	BATT_CUTOFF, /* latched until a charger appears */
};

struct cutoff {
	struct cutoff_cfg cfg;
	enum batt_state state;
	uint8_t below;
};

void cutoff_init(struct cutoff *c, const struct cutoff_cfg *cfg);

/*
 * Feed each new VCELL reading. vbus is the debounced dock state: on the dock
 * the charger holds VBAT up and wakes the pod from ship mode, so there is no
 * cut-off there.
 */
enum batt_state cutoff_update(struct cutoff *c, bool vbus, uint16_t vbat_mv);

/* A charger appeared with no new reading: release a latched cut-off. */
enum batt_state cutoff_charger_present(struct cutoff *c);

const char *batt_state_name(enum batt_state s);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_CUTOFF_H_ */
