/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include <stdlib.h>
#include <zephyr/ztest.h>

#include "ntc.h"

ZTEST_SUITE(ntc, NULL, NULL, NULL, NULL, NULL);

#define FS 4096

/* Code the SAADC would read at a given temperature, from the beta model. */
static int32_t code_at(double t_c)
{
	double r = 10000.0 * exp(3380.0 * (1.0 / (t_c + 273.15) - 1.0 / 298.15));

	return (int32_t)lround(FS * r / (r + 10000.0));
}

ZTEST(ntc, test_25c_is_half_scale)
{
	zassert_within(ntc_temp_cc(FS / 2, FS), 2500, 1);
}

ZTEST(ntc, test_policy_limits)
{
	/* the 12-bit code resolves the charge limits to better than 0.05 C */
	zassert_within(ntc_temp_cc(code_at(0.0), FS), 0, 5);
	zassert_within(ntc_temp_cc(code_at(10.0), FS), 1000, 5);
	zassert_within(ntc_temp_cc(code_at(45.0), FS), 4500, 5);
}

ZTEST(ntc, test_resolution_near_body_temperature)
{
	/* SCHEMATIC_SIM.md 2: 27 mK/LSB edge NTC; allow 50 mK */
	int32_t c = code_at(33.0);

	zassert_true(abs(ntc_temp_cc(c, FS) - ntc_temp_cc(c + 1, FS)) <= 5);
}

ZTEST(ntc, test_open_and_short)
{
	zassert_equal(ntc_temp_cc(0, FS), NTC_INVALID);
	zassert_equal(ntc_temp_cc(FS, FS), NTC_INVALID);
	zassert_equal(ntc_temp_cc(-12, FS), NTC_INVALID);
	zassert_equal(ntc_temp_cc(4090, FS), NTC_INVALID); /* > 200 kOhm: open */
	zassert_equal(ntc_resistance_ohm(FS / 2, FS), 10000);
}
