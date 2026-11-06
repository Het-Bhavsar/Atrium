/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pod power state machine: dock detection, charge policy, low-battery cut-off
 * and mode choice in one step function. The hardware glue (src/hw/power_hw.c)
 * gathers the inputs, calls power_sm_step() and applies the outputs.
 * Behaviour matches hardware/simulation logic_sim.py. Plain C, no Zephyr.
 */
#ifndef ATRIUM_POWER_SM_H_
#define ATRIUM_POWER_SM_H_

#include <stdbool.h>
#include <stdint.h>

#include "charge_policy.h"
#include "cutoff.h"
#include "dock.h"
#include "modes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum pod_state {
	POD_WORN = 0,    /* off the dock, measuring */
	POD_DOCKED,      /* on the dock: docked-idle, charger under the policy */
	POD_SHIPPING,    /* cut-off reached: firmware drives SHIP and the pod powers down */
	POD_OFFBODY,     /* off the dock, not worn: slow probe only */
};

struct power_sm_cfg {
	struct charge_policy_cfg charge;
	struct cutoff_cfg cutoff;
	struct dock_cfg dock;
};

#define POWER_SM_CFG_DEFAULT                                                      \
	{                                                                         \
		.charge = CHARGE_POLICY_CFG_DEFAULT, .cutoff = CUTOFF_CFG_DEFAULT,  \
		.dock = DOCK_CFG_DEFAULT,                                          \
	}

struct power_sm_in {
	uint32_t now_ms;
	bool vbus_sense_valid;
	uint32_t vbus_sense_mv; /* at the AIN0 pin */
	bool vbat_valid;        /* a new VCELL reading since the last step */
	uint16_t vbat_mv;
	struct charge_temps temps;
	bool chrg_n_low;
	enum pod_mode user_mode;
	bool night;
	bool worn;       /* wear detection result (wear.c); ignored on the dock */
};

struct power_sm_out {
	enum pod_state state;
	enum pod_mode mode;
	bool docked;
	bool chg_inhibit; /* drive CHG_INH low */
	bool ship;        /* drive SHIP high and keep it high */
	enum batt_state batt;
	enum chg_state chg;
	enum charge_band band;
};

struct power_sm {
	struct charge_policy charge;
	struct cutoff cutoff;
	struct dock dock;
	enum pod_state state;
};

void power_sm_init(struct power_sm *sm, const struct power_sm_cfg *cfg);
void power_sm_step(struct power_sm *sm, const struct power_sm_in *in, struct power_sm_out *out);

/* Mode for a given state: pure, exposed for tests. */
enum pod_mode power_sm_pick_mode(enum pod_state state, enum batt_state batt,
				 enum pod_mode user_mode, bool night);

const char *pod_state_name(enum pod_state s);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_POWER_SM_H_ */
