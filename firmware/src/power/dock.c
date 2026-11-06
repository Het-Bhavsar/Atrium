/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "dock.h"

/* On the dock with charging allowed, the BQ25101 charges whenever VBAT is under its recharge
 * threshold, VREG - 95 mV = 4.07 to 4.13 V (VREG 4.16 to 4.23 V, SCHEMATIC_SIM.md). Below 4.10 V
 * the cell is taking charge even when CHG# stays high after the first cycle. [ASSUMED] */
#define CHG_FULL_MV 4100

void dock_init(struct dock *d, const struct dock_cfg *cfg)
{
	d->cfg = *cfg;
	d->docked = false;
	d->count = 0;
}

uint32_t dock_vbus_mv(const struct dock_cfg *cfg, uint32_t sense_mv)
{
	uint64_t num = (uint64_t)sense_mv * ((uint64_t)cfg->r_top_ohm + cfg->r_bot_ohm);

	return cfg->r_bot_ohm ? (uint32_t)(num / cfg->r_bot_ohm) : 0;
}

bool dock_update(struct dock *d, uint32_t sense_mv)
{
	uint32_t vbus = dock_vbus_mv(&d->cfg, sense_mv);
	bool towards = d->docked ? vbus < d->cfg.off_vbus_mv : vbus > d->cfg.on_vbus_mv;

	if (!towards) {
		d->count = 0;
		return d->docked;
	}
	if (++d->count >= d->cfg.debounce) {
		d->docked = !d->docked;
		d->count = 0;
	}
	return d->docked;
}

enum chg_state chg_state_classify(bool docked, enum charge_band band, bool chrg_n_low,
				  uint16_t vbat_mv)
{
	if (!docked) {
		return CHG_UNDOCKED;
	}
	switch (band) {
	case CHG_BAND_NORMAL:
		break;
	case CHG_BAND_COLD:
		return CHG_COLD_LIMITED;
	default:
		return CHG_TEMP_BLOCKED;
	}
	if (chrg_n_low || (vbat_mv && vbat_mv < CHG_FULL_MV)) {
		return CHG_CHARGING;
	}
	return CHG_FULL;
}

const char *chg_state_name(enum chg_state s)
{
	static const char *const names[] = {"undocked", "charging", "full", "cold-limited",
					    "temp-blocked"};

	return (unsigned int)s < sizeof(names) / sizeof(names[0]) ? names[s] : "?";
}
