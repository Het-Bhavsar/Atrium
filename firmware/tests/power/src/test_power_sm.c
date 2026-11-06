/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "power_sm.h"

static struct power_sm sm;
static struct power_sm_in in;
static struct power_sm_out out;

#define SENSE_DOCKED   1520 /* 4.75 V VBUS */
#define SENSE_UNDOCKED 0

static void before(void *f)
{
	ARG_UNUSED(f);
	struct power_sm_cfg cfg = POWER_SM_CFG_DEFAULT;

	power_sm_init(&sm, &cfg);
	in = (struct power_sm_in){
		.now_ms = 1000,
		.vbus_sense_valid = true,
		.vbus_sense_mv = SENSE_UNDOCKED,
		.vbat_valid = true,
		.vbat_mv = 3900,
		.temps = {.primary = {.valid = true, .cc = 3300},
			  .backup = {.valid = true, .cc = 3200}},
		.user_mode = MODE_SMART,
		.worn = true,
	};
}

ZTEST_SUITE(power_sm, NULL, NULL, before, NULL, NULL);

static void step(int n)
{
	for (int i = 0; i < n; i++) {
		in.now_ms += 1000;
		in.temps.primary.at_ms = in.now_ms;
		in.temps.backup.at_ms = in.now_ms;
		power_sm_step(&sm, &in, &out);
	}
}

ZTEST(power_sm, test_worn_uses_user_mode)
{
	step(1);
	zassert_equal(out.state, POD_WORN);
	zassert_equal(out.mode, MODE_SMART);
	zassert_false(out.ship);
	zassert_false(out.chg_inhibit);
	zassert_equal(out.chg, CHG_UNDOCKED);
}

ZTEST(power_sm, test_night_and_low_battery)
{
	in.night = true;
	step(1);
	zassert_equal(out.mode, MODE_SLEEP);
	in.night = false;
	in.vbat_mv = 3420;
	step(1);
	zassert_equal(out.batt, BATT_LOW);
	zassert_equal(out.mode, MODE_LITE);
	in.user_mode = MODE_FULL;
	step(1);
	zassert_equal(out.mode, MODE_LITE);
}

ZTEST(power_sm, test_cutoff_ships_and_dock_wakes)
{
	in.vbat_mv = 3280;
	step(3);
	zassert_equal(out.state, POD_SHIPPING);
	zassert_true(out.ship);

	/* the charger wakes the LPB1010: firmware boots on the dock */
	in.vbus_sense_mv = SENSE_DOCKED;
	in.vbat_valid = false;
	step(2);
	zassert_equal(out.state, POD_DOCKED);
	zassert_false(out.ship);
	zassert_equal(out.mode, MODE_DOCKED_IDLE);
	zassert_equal(out.batt, BATT_LOW);
}

ZTEST(power_sm, test_docked_idle_and_charge_state)
{
	in.vbus_sense_mv = SENSE_DOCKED;
	in.chrg_n_low = true;
	step(2);
	zassert_true(out.docked);
	zassert_equal(out.mode, MODE_DOCKED_IDLE);
	zassert_equal(out.chg, CHG_CHARGING);

	in.chrg_n_low = false;
	in.vbat_mv = 4180;
	step(1);
	zassert_equal(out.chg, CHG_FULL);
}

ZTEST(power_sm, test_hot_dock_inhibits)
{
	in.vbus_sense_mv = SENSE_DOCKED;
	in.temps.backup.cc = 5000;
	in.temps.primary.cc = 4900;
	step(2);
	zassert_true(out.chg_inhibit);
	zassert_equal(out.chg, CHG_TEMP_BLOCKED);
	zassert_equal(out.band, CHG_BAND_HOT);

	/* lifted while hot: pin released */
	in.vbus_sense_mv = SENSE_UNDOCKED;
	step(2);
	zassert_false(out.docked);
	zassert_false(out.chg_inhibit);
}

ZTEST(power_sm, test_no_cutoff_while_docked)
{
	in.vbus_sense_mv = SENSE_DOCKED;
	in.vbat_mv = 3100;
	step(10);
	zassert_false(out.ship);
	zassert_equal(out.state, POD_DOCKED);
}

ZTEST(power_sm, test_off_body_uses_probe_mode)
{
	in.worn = false;
	step(1);
	zassert_equal(out.state, POD_OFFBODY);
	zassert_equal(out.mode, MODE_OFFBODY);
	/* the cut-off still applies off the body */
	in.vbat_mv = 3280;
	step(3);
	zassert_equal(out.state, POD_SHIPPING);
	zassert_true(out.ship);
}

ZTEST(power_sm, test_invalid_user_mode_falls_back)
{
	zassert_equal(power_sm_pick_mode(POD_WORN, BATT_OK, MODE_DOCKED_IDLE, false), MODE_SMART);
	zassert_equal(power_sm_pick_mode(POD_SHIPPING, BATT_CUTOFF, MODE_FULL, false),
		      MODE_DOCKED_IDLE);
	zassert_str_equal(pod_state_name(POD_DOCKED), "docked");
}
