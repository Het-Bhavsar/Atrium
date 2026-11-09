/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pod power state machine: dock detection, charge policy, low-battery cut-off
 * and mode choice in one step function. src/main.c gathers the inputs from
 * src/hw/power_hw.c and src/hw/charger.c, calls power_sm_step() and applies the
 * outputs. Plain C, no Zephyr.
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
	POD_POWEROFF,    /* cut-off reached: the pod goes to System OFF until the Plinth */
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
	bool pg_low;            /* /PG low: input present */
	bool vsys_valid;        /* a new VSYS reading since the last step */
	uint16_t vsys_mv;
	struct charge_temps temps;
	struct chg_report charger;
	enum pod_mode user_mode;
	bool night;
	bool worn;       /* wear detection result (wear.c); ignored on the dock */
	bool fit;        /* fit guide running (PPG Control bit 2): smart mode, green at 25 Hz */
	bool green_low;  /* VSYS below the green headroom limit (headroom.h): research stops */
};

struct power_sm_out {
	enum pod_state state;
	enum pod_mode mode;
	bool docked;
	bool chg_disable; /* BQ21061 CHARGER_DISABLE */
	bool chg_cold;    /* BQ21061 ICHG at the cold current */
	bool poweroff;    /* enter System OFF */
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
