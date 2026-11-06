/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "power_sm.h"

void power_sm_init(struct power_sm *sm, const struct power_sm_cfg *cfg)
{
	charge_policy_init(&sm->charge, &cfg->charge);
	cutoff_init(&sm->cutoff, &cfg->cutoff);
	dock_init(&sm->dock, &cfg->dock);
	sm->state = POD_WORN;
}

enum pod_mode power_sm_pick_mode(enum pod_state state, enum batt_state batt,
				 enum pod_mode user_mode, bool night)
{
	if (state == POD_OFFBODY) {
		return MODE_OFFBODY;
	}
	if (state != POD_WORN) {
		return MODE_DOCKED_IDLE;
	}
	if (user_mode >= MODE_DOCKED_IDLE) {
		user_mode = MODE_SMART;
	}
	if (night) {
		return mode_sleep_of(user_mode);
	}
	if (batt != BATT_OK) {
		return mode_lite_of(user_mode);
	}
	return user_mode;
}

void power_sm_step(struct power_sm *sm, const struct power_sm_in *in, struct power_sm_out *out)
{
	bool docked = sm->dock.docked;

	if (in->vbus_sense_valid) {
		docked = dock_update(&sm->dock, in->vbus_sense_mv);
	}

	enum batt_state batt = sm->cutoff.state;

	if (in->vbat_valid) {
		batt = cutoff_update(&sm->cutoff, docked, in->vbat_mv);
	} else if (docked) {
		batt = cutoff_charger_present(&sm->cutoff);
	}

	bool inhibit = charge_policy_update(&sm->charge, in->now_ms, docked, &in->temps);

	if (docked) {
		/* VBUS wakes the LPB1010 from ship mode, so the dock always wins */
		sm->state = POD_DOCKED;
	} else if (batt == BATT_CUTOFF) {
		sm->state = POD_SHIPPING;
	} else {
		sm->state = in->worn ? POD_WORN : POD_OFFBODY;
	}

	out->state = sm->state;
	out->docked = docked;
	out->batt = batt;
	out->band = sm->charge.band;
	out->chg_inhibit = inhibit;
	out->ship = sm->state == POD_SHIPPING;
	out->chg = chg_state_classify(docked, sm->charge.band, in->chrg_n_low, in->vbat_mv);
	out->mode = power_sm_pick_mode(sm->state, batt, in->user_mode, in->night);
}

const char *pod_state_name(enum pod_state s)
{
	switch (s) {
	case POD_WORN:
		return "worn";
	case POD_DOCKED:
		return "docked";
	case POD_SHIPPING:
		return "shipping";
	case POD_OFFBODY:
		return "off-body";
	}
	return "?";
}
