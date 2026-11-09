/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * ntc.c against Murata's NCP03XH103F05RL resistance-temperature table (NC-R8021, centre
 * values): the table points themselves, interpolation between them, the charge-policy
 * limits through the 12-bit SAADC, and open or shorted sensors.
 */
#include <math.h>
#include <stdlib.h>
#include <zephyr/ztest.h>

#include "ntc.h"

ZTEST_SUITE(ntc, NULL, NULL, NULL, NULL, NULL);

#define FS 4096

/* the SAADC code for an NTC resistance under the 10 kOhm divider resistor */
static int32_t code_of_ohm(double r)
{
	return (int32_t)lround(FS * r / (r + 10000.0));
}

ZTEST(ntc, test_murata_points)
{
	/* Murata centre values, ohms */
	static const struct {
		int32_t t_c;
		uint32_t r;
	} pts[] = {{-40, 195652}, {-10, 42506}, {0, 27219}, {10, 17926}, {25, 10000},
		   {30, 8315},    {37, 6475},   {45, 4917}, {85, 1452}};

	for (size_t i = 0; i < ARRAY_SIZE(pts); i++) {
		zassert_within(ntc_temp_cc_of_ohm(pts[i].r), pts[i].t_c * 100, 1, "%d C",
			       pts[i].t_c);
	}
}

ZTEST(ntc, test_interpolates_between_points)
{
	/* 36.5 C sits between 6.7067 kOhm (36 C) and 6.4751 kOhm (37 C) */
	zassert_within(ntc_temp_cc_of_ohm((6707 + 6475) / 2), 3650, 2);
	/* falls monotonically across the whole table */
	int32_t prev = INT32_MIN;

	for (uint32_t r = 195000; r > 1500; r = r * 99 / 100) {
		int32_t t = ntc_temp_cc_of_ohm(r);

		zassert_true(t > prev, "%u ohm", r);
		prev = t;
	}
}

ZTEST(ntc, test_25c_is_half_scale)
{
	zassert_equal(ntc_resistance_ohm(FS / 2, FS), 10000);
	zassert_within(ntc_temp_cc(FS / 2, FS), 2500, 1);
}

ZTEST(ntc, test_policy_limits)
{
	/* the 12-bit code resolves the charge limits to better than 0.1 C */
	zassert_within(ntc_temp_cc(code_of_ohm(27219), FS), 0, 10);
	zassert_within(ntc_temp_cc(code_of_ohm(17926), FS), 1000, 10);
	zassert_within(ntc_temp_cc(code_of_ohm(4917), FS), 4500, 10);
}

ZTEST(ntc, test_resolution_near_body_temperature)
{
	/* about 30 mK per code near 33 C; allow 50 mK */
	int32_t c = code_of_ohm(7461); /* 33 C */

	zassert_true(abs(ntc_temp_cc(c, FS) - ntc_temp_cc(c + 1, FS)) <= 5);
}

ZTEST(ntc, test_open_short_and_range)
{
	zassert_equal(ntc_temp_cc(0, FS), NTC_INVALID, "shorted");
	zassert_equal(ntc_temp_cc(FS, FS), NTC_INVALID, "open");
	zassert_equal(ntc_temp_cc(-12, FS), NTC_INVALID);
	zassert_equal(ntc_temp_cc(4090, FS), NTC_INVALID, "above 195 kOhm: below -40 C");
	zassert_equal(ntc_temp_cc_of_ohm(1400), NTC_INVALID, "above 85 C");
	zassert_equal(ntc_temp_cc_of_ohm(195652), -4000);
	zassert_equal(ntc_temp_cc_of_ohm(1452), 8500);
}
