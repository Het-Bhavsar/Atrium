/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "power_sm.h"

static struct power_sm sm;
static struct power_sm_in in;
static struct power_sm_out out;

static void before(void *f)
{
	ARG_UNUSED(f);
	struct power_sm_cfg cfg = POWER_SM_CFG_DEFAULT;

	power_sm_init(&sm, &cfg);
	in = (struct power_sm_in){
		.now_ms = 1000,
		.pg_low = false,
		.vsys_valid = true,
		.vsys_mv = 3900,
		.temps = {.primary = {.valid = true, .cc = 3300}},
		.charger = {.valid = true},
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
		power_sm_step(&sm, &in, &out);
	}
}

ZTEST(power_sm, test_worn_uses_user_mode)
{
	step(1);
	zassert_equal(out.state, POD_WORN);
	zassert_equal(out.mode, MODE_SMART);
	zassert_false(out.poweroff);
	zassert_false(out.chg_disable);
	zassert_equal(out.chg, CHG_UNDOCKED);
}

ZTEST(power_sm, test_night_and_low_battery)
{
	in.night = true;
	step(1);
	zassert_equal(out.mode, MODE_SLEEP);
	in.night = false;
	in.vsys_mv = 3420;
	step(1);
	zassert_equal(out.batt, BATT_LOW);
	zassert_equal(out.mode, MODE_LITE);
	in.user_mode = MODE_FULL;
	step(1);
	zassert_equal(out.mode, MODE_LITE);
}

ZTEST(power_sm, test_cutoff_powers_off_and_dock_releases)
{
	in.vsys_mv = 3280;
	step(3);
	zassert_equal(out.state, POD_POWEROFF);
	zassert_true(out.poweroff);

	/* /PG wakes the nRF from System OFF: the pod boots on the Plinth */
	in.pg_low = true;
	in.vsys_valid = false;
	step(2);
	zassert_equal(out.state, POD_DOCKED);
	zassert_false(out.poweroff);
	zassert_equal(out.mode, MODE_DOCKED_IDLE);
	zassert_equal(out.batt, BATT_LOW);
}

ZTEST(power_sm, test_docked_idle_and_charge_state)
{
	in.pg_low = true;
	step(2);
	zassert_true(out.docked);
	zassert_equal(out.mode, MODE_DOCKED_IDLE);
	zassert_equal(out.chg, CHG_CHARGING);

	in.charger.done = true;
	step(1);
	zassert_equal(out.chg, CHG_FULL);
}

ZTEST(power_sm, test_cold_dock_lowers_current)
{
	in.pg_low = true;
	in.temps.primary.cc = 500;
	step(2);
	zassert_true(out.chg_cold);
	zassert_false(out.chg_disable);
	zassert_equal(out.chg, CHG_COLD_LIMITED);
}

ZTEST(power_sm, test_hot_dock_disables)
{
	in.pg_low = true;
	in.temps.primary.cc = 4900;
	step(2);
	zassert_true(out.chg_disable);
	zassert_equal(out.chg, CHG_TEMP_BLOCKED);
	zassert_equal(out.band, CHG_BAND_HOT);

	/* lifted while hot: nothing to disable */
	in.pg_low = false;
	step(2);
	zassert_false(out.docked);
	zassert_false(out.chg_disable);
}

/* On the Plinth VSYS is regulated at 4.5 V, or anything while the charger starts: it is
 * never a cell voltage, so it cannot cut the pod off */
ZTEST(power_sm, test_no_cutoff_while_docked)
{
	in.pg_low = true;
	in.vsys_mv = 3100;
	step(10);
	zassert_false(out.poweroff);
	zassert_equal(out.state, POD_DOCKED);
}

ZTEST(power_sm, test_off_body_uses_probe_mode)
{
	in.worn = false;
	step(1);
	zassert_equal(out.state, POD_OFFBODY);
	zassert_equal(out.mode, MODE_OFFBODY);
	/* the cut-off still applies off the body */
	in.vsys_mv = 3280;
	step(3);
	zassert_equal(out.state, POD_POWEROFF);
	zassert_true(out.poweroff);
}

ZTEST(power_sm, test_invalid_user_mode_falls_back)
{
	zassert_equal(power_sm_pick_mode(POD_WORN, BATT_OK, MODE_DOCKED_IDLE, false), MODE_SMART);
	zassert_equal(power_sm_pick_mode(POD_POWEROFF, BATT_CUTOFF, MODE_FULL, false),
		      MODE_DOCKED_IDLE);
	zassert_str_equal(pod_state_name(POD_DOCKED), "docked");
	zassert_str_equal(pod_state_name(POD_POWEROFF), "power-off");
}

ZTEST(power_sm, test_fit_holds_green_before_worn)
{
	/* the fit guide runs before wear detection says worn: green at 25 Hz, no duty */
	in.worn = false;
	in.fit = true;
	step(1);
	zassert_equal(out.state, POD_OFFBODY);
	zassert_equal(out.mode, MODE_SMART);
	/* and over a duty-cycled user mode, at night too */
	in.worn = true;
	in.user_mode = MODE_LITE;
	in.night = true;
	step(1);
	zassert_equal(out.mode, MODE_SMART);
	/* never on the Plinth */
	in.pg_low = true;
	step(3);
	zassert_equal(out.state, POD_DOCKED);
	zassert_equal(out.mode, MODE_DOCKED_IDLE);
	/* off again: back to the probe */
	in.pg_low = false;
	in.fit = false;
	in.worn = false;
	step(3);
	zassert_equal(out.mode, MODE_OFFBODY);
}

ZTEST(power_sm, test_research_mode_rules)
{
	/* worn with headroom: the research mode runs */
	in.user_mode = MODE_RESEARCH_200;
	step(1);
	zassert_equal(out.state, POD_WORN);
	zassert_equal(out.mode, MODE_RESEARCH_200);
	/* below the green limit it gives way to full, never to a shifted slot table */
	in.green_low = true;
	step(1);
	zassert_equal(out.mode, MODE_FULL);
	in.green_low = false;
	step(1);
	zassert_equal(out.mode, MODE_RESEARCH_200);
	/* night and low battery as for full */
	in.night = true;
	step(1);
	zassert_equal(out.mode, MODE_SLEEP);
	in.night = false;
	in.vsys_mv = 3420;
	step(1);
	zassert_equal(out.mode, MODE_LITE);
	in.vsys_mv = 3900;
	/* off the body: the probe */
	in.user_mode = MODE_RESEARCH_100;
	in.worn = false;
	step(1);
	zassert_equal(out.mode, MODE_OFFBODY);
	/* never on the Plinth */
	in.worn = true;
	in.pg_low = true;
	step(3);
	zassert_equal(out.state, POD_DOCKED);
	zassert_equal(out.mode, MODE_DOCKED_IDLE);
	zassert_equal(power_sm_pick_mode(POD_DOCKED, BATT_OK, MODE_RESEARCH_100, false),
		      MODE_DOCKED_IDLE);
	zassert_equal(power_sm_pick_mode(POD_WORN, BATT_OK, MODE_RESEARCH_100, false),
		      MODE_RESEARCH_100);
	zassert_equal(power_sm_pick_mode(POD_WORN, BATT_OK, MODE_COUNT, false), MODE_SMART);
	/* the averaged modes follow the same rules */
	zassert_equal(power_sm_pick_mode(POD_WORN, BATT_OK, MODE_RESEARCH_AVG50, false),
		      MODE_RESEARCH_AVG50);
	zassert_equal(power_sm_pick_mode(POD_DOCKED, BATT_OK, MODE_RESEARCH_AVG25, false),
		      MODE_DOCKED_IDLE);
	zassert_equal(power_sm_pick_mode(POD_WORN, BATT_LOW, MODE_RESEARCH_AVG25, false), MODE_LITE);
	in.pg_low = false;
	in.user_mode = MODE_RESEARCH_AVG50;
	in.green_low = true;
	step(3);
	zassert_equal(out.state, POD_WORN);
	zassert_equal(out.mode, MODE_FULL);
}

ZTEST(power_sm, test_vibration_mode_rules)
{
	in.user_mode = MODE_RESEARCH_VIB6K7;
	step(1);
	zassert_equal(out.state, POD_WORN);
	zassert_equal(out.mode, MODE_RESEARCH_VIB6K7);
	/* no LEDs: the green headroom limit does not move it to full */
	in.green_low = true;
	step(1);
	zassert_equal(out.mode, MODE_RESEARCH_VIB6K7);
	in.green_low = false;
	/* night and low battery as for the other research modes */
	in.night = true;
	step(1);
	zassert_equal(out.mode, MODE_SLEEP);
	in.night = false;
	in.vsys_mv = 3420;
	step(1);
	zassert_equal(out.mode, MODE_LITE);
	in.vsys_mv = 3900;
	in.user_mode = MODE_RESEARCH_VIB3K3;
	step(1);
	zassert_equal(out.mode, MODE_RESEARCH_VIB3K3);
	/* off the body: the probe; on the Plinth: docked idle */
	in.worn = false;
	step(1);
	zassert_equal(out.mode, MODE_OFFBODY);
	in.worn = true;
	in.pg_low = true;
	step(3);
	zassert_equal(out.mode, MODE_DOCKED_IDLE);
	zassert_equal(power_sm_pick_mode(POD_DOCKED, BATT_OK, MODE_RESEARCH_VIB6K7, false),
		      MODE_DOCKED_IDLE);
	/* the fit guide still wins */
	in.pg_low = false;
	step(3);
	in.fit = true;
	step(1);
	zassert_equal(out.mode, MODE_SMART);
	in.fit = false;
}
