/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "cutoff.h"

static struct cutoff c;

static void before(void *f)
{
	ARG_UNUSED(f);
	struct cutoff_cfg cfg = CUTOFF_CFG_DEFAULT;

	cutoff_init(&c, &cfg);
}

ZTEST_SUITE(cutoff, NULL, NULL, before, NULL, NULL);

/* Cut-off above LPB1010 VDL max 2.90 V. Rev B.2: under a 30 mA peak (a 15 mA LED pulse plus
 * radio TX) a 2.0 ohm cell [ASSUMED] sags 60 mV, and 3.24 V still leaves the red LED (Vf
 * 2.5 V max at 20 mA, MAXM86161 data sheet) about 0.7 V of driver compliance. Green is
 * already off below green-min-mv (headroom.c). */
ZTEST(cutoff, test_limits_hold_rev_b2_headroom)
{
	zassert_equal(c.cfg.cutoff_mv, 3300);
	zassert_true(c.cfg.cutoff_mv > 2900);
	zassert_true(c.cfg.cutoff_mv - 30 * 2000 / 1000 - 2500 >= 700);
}

ZTEST(cutoff, test_needs_three_readings_in_a_row)
{
	zassert_equal(cutoff_update(&c, false, 3290), BATT_LOW);
	zassert_equal(cutoff_update(&c, false, 3290), BATT_LOW);
	zassert_equal(cutoff_update(&c, false, 3290), BATT_CUTOFF);
}

ZTEST(cutoff, test_burst_droop_does_not_trip)
{
	/* a reading inside an LED burst, then the cell recovers */
	for (int i = 0; i < 20; i++) {
		cutoff_update(&c, false, (i % 2) ? 3280 : 3310);
	}
	zassert_not_equal(c.state, BATT_CUTOFF);
}

ZTEST(cutoff, test_exactly_at_limit_is_not_below)
{
	for (int i = 0; i < 10; i++) {
		cutoff_update(&c, false, 3300);
	}
	zassert_equal(c.state, BATT_LOW);
}

ZTEST(cutoff, test_no_cutoff_on_dock)
{
	for (int i = 0; i < 10; i++) {
		zassert_not_equal(cutoff_update(&c, true, 3000), BATT_CUTOFF);
	}
}

ZTEST(cutoff, test_cutoff_latches_until_charger)
{
	for (int i = 0; i < 3; i++) {
		cutoff_update(&c, false, 3200);
	}
	zassert_equal(c.state, BATT_CUTOFF);
	/* load drops as the pod shuts down and VCELL rebounds: still shipping */
	zassert_equal(cutoff_update(&c, false, 3600), BATT_CUTOFF);
	/* the dock releases it */
	zassert_equal(cutoff_update(&c, true, 3400), BATT_LOW);
	zassert_equal(cutoff_update(&c, true, 3700), BATT_OK);
}

ZTEST(cutoff, test_charger_present_without_reading)
{
	for (int i = 0; i < 3; i++) {
		cutoff_update(&c, false, 3200);
	}
	zassert_equal(cutoff_charger_present(&c), BATT_LOW);
}

ZTEST(cutoff, test_invalid_readings_ignored)
{
	cutoff_update(&c, false, 3200);
	cutoff_update(&c, false, 0);     /* gauge not ready */
	cutoff_update(&c, false, 65535); /* bus error */
	cutoff_update(&c, false, 3200);
	zassert_equal(c.state, BATT_LOW);
	zassert_equal(cutoff_update(&c, false, 3200), BATT_CUTOFF);
}

ZTEST(cutoff, test_low_battery_hysteresis)
{
	zassert_equal(cutoff_update(&c, false, 3800), BATT_OK);
	zassert_equal(cutoff_update(&c, false, 3440), BATT_LOW);
	zassert_equal(cutoff_update(&c, false, 3500), BATT_LOW);
	zassert_equal(cutoff_update(&c, false, 3560), BATT_OK);
}

/* Discharge like logic_sim.py: OCV curve minus I*R, VCELL every 45 s (hibernate cadence).
 * Ship mode must start before the loaded voltage gets near the LPB1010 trip. */
ZTEST(cutoff, test_discharge_reaches_ship_above_vdl)
{
	static const float soc_t[] = {0, .03f, .06f, .1f, .2f};
	static const float ocv_t[] = {3.00f, 3.35f, 3.50f, 3.60f, 3.69f};
	float soc = 0.2f, cap_mah = 32.0f, i_ma = 0.61f, r = 0.8f; /* aged cell, smart worst case */
	float v = 0;
	int steps = 0;

	while (c.state != BATT_CUTOFF && soc > 0 && steps < 100000) {
		int k = 0;

		while (k < 3 && soc > soc_t[k + 1]) {
			k++;
		}
		float ocv = ocv_t[k] + (ocv_t[k + 1] - ocv_t[k]) * (soc - soc_t[k]) / (soc_t[k + 1] - soc_t[k]);

		v = ocv - i_ma * 1e-3f * r;
		cutoff_update(&c, false, (uint16_t)(v * 1000.0f));
		soc -= i_ma * (45.0f / 3600.0f) / cap_mah;
		steps++;
	}
	zassert_equal(c.state, BATT_CUTOFF);
	zassert_true(v > 3.25f, "ship started at %.3f V", (double)v);
	zassert_true(v < 3.30f);
}
