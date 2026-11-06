/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include <zephyr/ztest.h>

#include "dock.h"

static struct dock d;

static void before(void *f)
{
	ARG_UNUSED(f);
	struct dock_cfg cfg = DOCK_CFG_DEFAULT;

	dock_init(&d, &cfg);
}

ZTEST_SUITE(dock, NULL, NULL, before, NULL, NULL);

static uint32_t sense_of(uint32_t vbus_mv)
{
	return vbus_mv * 470 / 1470;
}

ZTEST(dock, test_divider)
{
	/* 4.90 V on the dock gives 1.57 V at AIN0 (SCHEMATIC_SIM.md 5.4 item 5) */
	zassert_within(sense_of(4900), 1567, 1);
	zassert_within(dock_vbus_mv(&d.cfg, 1567), 4900, 3);
}

ZTEST(dock, test_docks_at_lowest_case_voltage)
{
	/* pod VBUS at 30 mA is 4.54 to 4.87 V (SCHEMATIC_SIM.md 2) */
	zassert_false(dock_update(&d, sense_of(4540)));
	zassert_true(dock_update(&d, sense_of(4540)));
}

ZTEST(dock, test_single_spike_ignored)
{
	dock_update(&d, sense_of(4800));
	dock_update(&d, 0);
	zassert_false(dock_update(&d, sense_of(4800)));
	zassert_true(dock_update(&d, sense_of(4800)));
}

ZTEST(dock, test_hysteresis_holds)
{
	dock_update(&d, sense_of(4800));
	dock_update(&d, sense_of(4800));
	for (int i = 0; i < 10; i++) {
		zassert_true(dock_update(&d, sense_of(3700)));
	}
	before(NULL);
	for (int i = 0; i < 10; i++) {
		zassert_false(dock_update(&d, sense_of(3700)));
	}
}

/* Lift: C1 1 uF bleeds through R14 + R15 (1.47 s) at the slowest, VBUS from 4.73 V.
 * Sampled every 100 ms, the lift is seen in under a second. */
ZTEST(dock, test_lift_detected_within_one_second)
{
	dock_update(&d, sense_of(4730));
	dock_update(&d, sense_of(4730));
	zassert_true(d.docked);

	int ms = 0;

	while (d.docked && ms < 5000) {
		ms += 100;
		uint32_t vbus = (uint32_t)(4730.0 * exp(-ms / 1470.0));

		dock_update(&d, sense_of(vbus));
	}
	zassert_false(d.docked);
	zassert_true(ms <= 1000, "lift seen after %d ms", ms);
}

ZTEST(dock, test_chg_state)
{
	zassert_equal(chg_state_classify(false, CHG_BAND_NORMAL, true, 3800), CHG_UNDOCKED);
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, true, 4180), CHG_CHARGING);
	/* recharge cycle: CHG# stays high, but VBAT is under the recharge threshold */
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, false, 4050), CHG_CHARGING);
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, false, 4150), CHG_FULL);
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, false, 0), CHG_FULL, "no reading yet");
	zassert_equal(chg_state_classify(true, CHG_BAND_COLD, true, 3800), CHG_COLD_LIMITED);
	zassert_equal(chg_state_classify(true, CHG_BAND_HOT, true, 3800), CHG_TEMP_BLOCKED);
	zassert_equal(chg_state_classify(true, CHG_BAND_FAULT, false, 3800), CHG_TEMP_BLOCKED);
	zassert_str_equal(chg_state_name(CHG_FULL), "full");
}
