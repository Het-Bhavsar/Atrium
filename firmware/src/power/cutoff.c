/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cutoff.h"

void cutoff_init(struct cutoff *c, const struct cutoff_cfg *cfg)
{
	c->cfg = *cfg;
	c->state = BATT_OK;
	c->below = 0;
}

enum batt_state cutoff_update(struct cutoff *c, bool vbus, uint16_t vbat_mv)
{
	const struct cutoff_cfg *k = &c->cfg;

	if (vbat_mv < k->min_valid_mv || vbat_mv > k->max_valid_mv) {
		return c->state; /* not a cell voltage: no decision */
	}

	if (vbus) {
		cutoff_charger_present(c);
	} else if (c->state != BATT_CUTOFF) {
		if (vbat_mv < k->cutoff_mv) {
			if (++c->below >= k->confirm) {
				c->state = BATT_CUTOFF;
				return c->state;
			}
		} else {
			c->below = 0;
		}
	} else {
		return c->state;
	}

	if (vbat_mv < k->low_mv) {
		c->state = BATT_LOW;
	} else if (vbat_mv > k->low_clear_mv) {
		c->state = BATT_OK;
	}
	return c->state;
}

enum batt_state cutoff_charger_present(struct cutoff *c)
{
	c->below = 0;
	if (c->state == BATT_CUTOFF) {
		c->state = BATT_LOW;
	}
	return c->state;
}

const char *batt_state_name(enum batt_state s)
{
	switch (s) {
	case BATT_OK:
		return "ok";
	case BATT_LOW:
		return "low";
	case BATT_CUTOFF:
		return "cutoff";
	}
	return "?";
}
