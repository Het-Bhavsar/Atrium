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
	if (!mode_user_selectable(user_mode)) {
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
	bool docked = dock_update(&sm->dock, in->pg_low);
	enum batt_state batt = sm->cutoff.state;

	if (in->vsys_valid && !docked) {
		batt = cutoff_update(&sm->cutoff, false, in->vsys_mv);
	} else if (docked) {
		batt = cutoff_charger_present(&sm->cutoff);
	}

	bool inhibit = charge_policy_update(&sm->charge, in->now_ms, docked, &in->temps);

	if (docked) {
		/* the charger powers the pod, so the dock always wins */
		sm->state = POD_DOCKED;
	} else if (batt == BATT_CUTOFF) {
		sm->state = POD_POWEROFF;
	} else {
		sm->state = in->worn ? POD_WORN : POD_OFFBODY;
	}

	out->state = sm->state;
	out->docked = docked;
	out->batt = batt;
	out->band = sm->charge.band;
	out->chg_disable = inhibit;
	out->chg_cold = sm->charge.band == CHG_BAND_COLD;
	out->poweroff = sm->state == POD_POWEROFF;
	out->chg = chg_state_classify(docked, sm->charge.band, &in->charger);
	out->mode = power_sm_pick_mode(sm->state, batt, in->user_mode, in->night);
	/* below the green limit a research mode would lose its green slot and every tag after
	 * it would shift; full keeps IR and red at 25 Hz, and the stream's mode byte says so.
	 * The vibration modes have no LEDs, so they keep running */
	if (in->green_low && mode_is_research(out->mode) && !mode_is_vibration(out->mode)) {
		out->mode = MODE_FULL;
	}
	/* the fit guide needs a continuous green pulse before wear detection says worn */
	if (in->fit && (sm->state == POD_WORN || sm->state == POD_OFFBODY)) {
		out->mode = MODE_SMART;
	}
}

const char *pod_state_name(enum pod_state s)
{
	switch (s) {
	case POD_WORN:
		return "worn";
	case POD_DOCKED:
		return "docked";
	case POD_POWEROFF:
		return "power-off";
	case POD_OFFBODY:
		return "off-body";
	}
	return "?";
}
