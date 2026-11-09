/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Low-battery cut-off. Below 3.30 V loaded, off the dock, the pod powers down to nRF
 * System OFF with every sensor off, and wakes when it is next put on the Plinth (/PG).
 * Ship mode is not possible here: the BQ21061 answers I2C only with VIN present. The
 * AP6683 protection on the cell is the last guard below that.
 *
 * The voltage is VSYS (VSYS_SENSE, VSYS / 3.2 on AIN4). Off the Plinth the BQ21061 power
 * path connects the cell to VSYS, so it is the loaded cell voltage. On the Plinth VSYS is
 * regulated at 4.5 V and says nothing about the cell. A reading can land inside an LED
 * burst, so the cut-off needs several readings in a row below the limit. Plain C, no Zephyr.
 */
#ifndef ATRIUM_CUTOFF_H_
#define ATRIUM_CUTOFF_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cutoff_cfg {
	uint16_t cutoff_mv;    /* loaded VSYS that powers the pod down */
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
 * Feed each new VSYS reading. vbus is the debounced dock state: on the dock the
 * charger powers the pod, so there is no cut-off there.
 */
enum batt_state cutoff_update(struct cutoff *c, bool vbus, uint16_t vbat_mv);

/* A charger appeared with no new reading: release a latched cut-off. */
enum batt_state cutoff_charger_present(struct cutoff *c);

const char *batt_state_name(enum batt_state s);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_CUTOFF_H_ */
