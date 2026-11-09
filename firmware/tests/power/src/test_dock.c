/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
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

ZTEST(dock, test_pg_low_docks_after_debounce)
{
	zassert_false(dock_update(&d, true));
	zassert_true(dock_update(&d, true));
	zassert_true(dock_update(&d, true));
}

ZTEST(dock, test_single_bounce_ignored)
{
	dock_update(&d, true);
	dock_update(&d, false); /* the contact bounced as the pod seated */
	zassert_false(dock_update(&d, true));
	zassert_true(dock_update(&d, true));
	/* and on the way off */
	dock_update(&d, false);
	zassert_true(dock_update(&d, true));
	zassert_true(dock_update(&d, false));
	zassert_false(dock_update(&d, false));
}

static const struct chg_report ok = {.valid = true};

ZTEST(dock, test_chg_state)
{
	struct chg_report done = {.valid = true, .done = true};
	struct chg_report ts = {.valid = true, .ts_stop = true};
	struct chg_report fault = {.valid = true, .fault = true};
	struct chg_report none = {0};

	zassert_equal(chg_state_classify(false, CHG_BAND_NORMAL, &ok), CHG_UNDOCKED);
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, &ok), CHG_CHARGING);
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, &done), CHG_FULL);
	zassert_equal(chg_state_classify(true, CHG_BAND_COLD, &ok), CHG_COLD_LIMITED);
	zassert_equal(chg_state_classify(true, CHG_BAND_COLD, &done), CHG_FULL);
	/* the charger's own TS stopped it: the cell is out of range even if TH4 is not */
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, &ts), CHG_TEMP_BLOCKED);
	zassert_equal(chg_state_classify(true, CHG_BAND_HOT, &ok), CHG_TEMP_BLOCKED);
	zassert_equal(chg_state_classify(true, CHG_BAND_FAULT, &done), CHG_TEMP_BLOCKED);
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, &fault), CHG_FAULT);
	zassert_equal(chg_state_classify(true, CHG_BAND_NORMAL, &none), CHG_FAULT,
		      "no answer on I2C");
	zassert_str_equal(chg_state_name(CHG_FULL), "full");
	zassert_str_equal(chg_state_name(CHG_FAULT), "fault");
}
