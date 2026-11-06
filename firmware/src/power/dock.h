/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Dock detection from VBUS_SENSE and charge-state reporting.
 *
 * VBUS_SENSE is VBUS through R14 1 MOhm over R15 470 kOhm into P0.02/AIN0:
 * 4.54 to 4.90 V on the dock gives 1.45 to 1.57 V (SCHEMATIC_SIM.md 2 and 5.3 D).
 * After a lift, VBUS bleeds down through the divider and the BQ25101 input,
 * so absence needs the lower threshold for a few readings. CHG# only marks
 * the first charge cycle (5.4 item 3), so "charging" also uses VBAT against
 * the charger's recharge threshold. Plain C, no Zephyr.
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
	uint32_t r_top_ohm;  /* R14 */
	uint32_t r_bot_ohm;  /* R15 */
	uint16_t on_vbus_mv; /* VBUS above this counts towards docked */
	uint16_t off_vbus_mv;/* VBUS below this counts towards undocked */
	uint8_t debounce;    /* readings in a row to change state */
};

#define DOCK_CFG_DEFAULT                                                          \
	{                                                                         \
		.r_top_ohm = 1000000, .r_bot_ohm = 470000, .on_vbus_mv = 4000,     \
		.off_vbus_mv = 3500, .debounce = 2,                                \
	}

struct dock {
	struct dock_cfg cfg;
	bool docked;
	uint8_t count;
};

void dock_init(struct dock *d, const struct dock_cfg *cfg);

/* VBUS in mV from the voltage at the ADC pin. */
uint32_t dock_vbus_mv(const struct dock_cfg *cfg, uint32_t sense_mv);

/* Feed each VBUS_SENSE reading (mV at the pin). Returns the debounced state. */
bool dock_update(struct dock *d, uint32_t sense_mv);

enum chg_state {
	CHG_UNDOCKED = 0,
	CHG_CHARGING,
	CHG_FULL,         /* on the dock, no current into the cell */
	CHG_COLD_LIMITED, /* 0 to 10 C, duty-cycled to 0.2C */
	CHG_TEMP_BLOCKED, /* below 0 C, above 45 C, or no valid temperature */
};

/*
 * chrg_n_low: BQ25101 CHG# pulled low (first cycle only).
 * vbat_mv: the latest VBAT reading, 0 if none yet.
 */
enum chg_state chg_state_classify(bool docked, enum charge_band band, bool chrg_n_low,
				  uint16_t vbat_mv);

const char *chg_state_name(enum chg_state s);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_DOCK_H_ */
