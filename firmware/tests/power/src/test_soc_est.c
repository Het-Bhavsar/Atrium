/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "soc_est.h"

static struct soc s;

static void before(void *f)
{
	ARG_UNUSED(f);
	struct soc_cfg cfg = SOC_CFG_DEFAULT;

	soc_init(&s, &cfg);
}

ZTEST_SUITE(soc, NULL, NULL, before, NULL, NULL);

ZTEST(soc, test_ocv_curve)
{
	zassert_equal(soc_from_ocv_mv(2900), 0);
	zassert_equal(soc_from_ocv_mv(3820), 500);
	zassert_equal(soc_from_ocv_mv(4190), 1000);
	zassert_equal(soc_from_ocv_mv(4300), 1000);
	zassert_within(soc_from_ocv_mv(3980), 750, 1);
}

ZTEST(soc, test_unprimed_reads_zero)
{
	zassert_equal(soc_percent(&s), 0);
}

ZTEST(soc, test_first_reading_primes_with_ir_correction)
{
	/* 3.81 V under 5 mA through 2 ohm is 3.82 V open circuit: 50 % */
	soc_on_vbat(&s, 3810, 5000);
	zassert_equal(soc_percent(&s), 50);
}

ZTEST(soc, test_coulomb_count)
{
	soc_on_vbat(&s, 3820, 0); /* 50 % = 15 mAh */
	/* 0.30 mA for 10 h = 3 mAh = 10 % of 30 mAh */
	for (int i = 0; i < 36000; i++) {
		soc_flow(&s, 1000, 300, 0);
	}
	zassert_equal(soc_percent(&s), 40);
}

ZTEST(soc, test_charge_counts_up_and_full_snaps)
{
	soc_on_vbat(&s, 3690, 0); /* 20 % */
	for (int i = 0; i < 1800; i++) {
		soc_flow(&s, 1000, 20, 14850); /* 30 min at 14.85 mA: +7.4 mAh = +25 % */
	}
	zassert_within(soc_percent(&s), 45, 1);
	soc_full(&s);
	zassert_equal(soc_percent(&s), 100);
	soc_flow(&s, 1000, 0, 15000);
	zassert_equal(soc_percent(&s), 100, "no count above full");
}

ZTEST(soc, test_voltage_pulls_the_count_only_at_light_load)
{
	soc_on_vbat(&s, 3820, 0); /* the count says 50 % */
	/* the cell actually reads 3.69 V at rest: 20 % */
	for (int i = 0; i < 10; i++) {
		soc_on_vbat(&s, 3690, 900); /* heavy load: no correction */
	}
	zassert_equal(soc_percent(&s), 50);
	for (int i = 0; i < 300; i++) {
		soc_on_vbat(&s, 3690, 100);
	}
	zassert_within(soc_percent(&s), 20, 1);
}

ZTEST(soc, test_never_negative)
{
	soc_on_vbat(&s, 3350, 0);
	for (int i = 0; i < 100000; i++) {
		soc_flow(&s, 1000, 1000, 0);
	}
	zassert_equal(soc_percent(&s), 0);
}
