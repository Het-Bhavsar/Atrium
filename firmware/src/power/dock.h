/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Dock detection from the BQ21061 /PG pin, and charge-state reporting.
 *
 * /PG (P0.14, 100 kOhm pull-up) is low while the charger has a valid input, that is on
 * the Plinth. A contact can bounce as the pod seats, so a change needs a few readings in
 * a row. The charge state comes from the charger's own status (STAT0 / STAT1), read over
 * I2C, which works only on the Plinth. Plain C, no Zephyr.
 */
#ifndef ATRIUM_DOCK_H_
#define ATRIUM_DOCK_H_

#include <stdbool.h>
#include <stdint.h>

#include "charge_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dock_cfg {
	uint8_t debounce; /* readings in a row to change state */
};

#define DOCK_CFG_DEFAULT {.debounce = 2}

struct dock {
	struct dock_cfg cfg;
	bool docked;
	uint8_t count;
};

void dock_init(struct dock *d, const struct dock_cfg *cfg);

/* Feed each /PG reading (true = low = input present). Returns the debounced state. */
bool dock_update(struct dock *d, bool pg_low);

enum chg_state {
	CHG_UNDOCKED = 0,
	CHG_CHARGING,
	CHG_FULL,         /* on the dock, charge cycle done */
	CHG_COLD_LIMITED, /* 0 to 10 C: cold current */
	CHG_TEMP_BLOCKED, /* below 0 C, above 45 C, or no valid temperature */
	CHG_FAULT,        /* the charger does not answer, or reports a fault */
};

/* What the charger reported at its last service. */
struct chg_report {
	bool valid;   /* the I2C read succeeded */
	bool done;    /* STAT0 CHARGE_DONE */
	bool ts_stop; /* the charger's own TS stopped charging (cold or hot) */
	bool fault;   /* VIN over-voltage or battery over-current */
};

enum chg_state chg_state_classify(bool docked, enum charge_band band, const struct chg_report *r);

const char *chg_state_name(enum chg_state s);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_DOCK_H_ */
