/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "dock.h"

void dock_init(struct dock *d, const struct dock_cfg *cfg)
{
	d->cfg = *cfg;
	d->docked = false;
	d->count = 0;
}

bool dock_update(struct dock *d, bool pg_low)
{
	if (pg_low == d->docked) {
		d->count = 0;
		return d->docked;
	}
	if (++d->count >= d->cfg.debounce) {
		d->docked = pg_low;
		d->count = 0;
	}
	return d->docked;
}

enum chg_state chg_state_classify(bool docked, enum charge_band band, const struct chg_report *r)
{
	if (!docked) {
		return CHG_UNDOCKED;
	}
	if (!r->valid || r->fault) {
		return CHG_FAULT;
	}
	if (band != CHG_BAND_NORMAL && band != CHG_BAND_COLD) {
		return CHG_TEMP_BLOCKED;
	}
	if (r->done) {
		return CHG_FULL;
	}
	if (r->ts_stop) {
		return CHG_TEMP_BLOCKED;
	}
	return band == CHG_BAND_COLD ? CHG_COLD_LIMITED : CHG_CHARGING;
}

const char *chg_state_name(enum chg_state s)
{
	static const char *const names[] = {"undocked",     "charging",     "full",
					    "cold-limited", "temp-blocked", "fault"};

	return (unsigned int)s < sizeof(names) / sizeof(names[0]) ? names[s] : "?";
}
