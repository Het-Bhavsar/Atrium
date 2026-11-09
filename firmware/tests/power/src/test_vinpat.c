/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * The case's VIN patterns as the pod sees them on /PG. The timings are the case's
 * (firmware/case/src/case_sm.h: 200 ms off, 300 ms on); the case tests run the same
 * detector against the case state machine's own output.
 */
#include <zephyr/ztest.h>

#include "vinpat.h"

static struct vinpat v;

static void before(void *f)
{
	ARG_UNUSED(f);
	static const struct vinpat_cfg cfg = VINPAT_CFG_DEFAULT;

	vinpat_init(&v, &cfg, true);
}

ZTEST_SUITE(vinpat, NULL, NULL, before, NULL, NULL);

/* VIN off at t for `off` ms: the event at VIN's return */
static enum vinpat_ev pulse(uint32_t t, uint32_t off)
{
	zassert_equal(vinpat_edge(&v, false, t), VINPAT_NONE);
	return vinpat_edge(&v, true, t + off);
}

ZTEST(vinpat, test_soft_reset_pattern)
{
	zassert_equal(pulse(10000, 200), VINPAT_NONE);
	zassert_equal(pulse(10500, 200), VINPAT_CASE, "two pulses read as the signature");
	zassert_equal(pulse(11000, 200), VINPAT_RESET);
}

ZTEST(vinpat, test_signature_marks_case_until_a_long_drop)
{
	zassert_false(vinpat_on_case(&v));
	zassert_equal(pulse(10000, 200), VINPAT_NONE);
	zassert_equal(pulse(10500, 200), VINPAT_CASE);
	zassert_true(vinpat_on_case(&v));

	zassert_equal(vinpat_edge(&v, false, 20000), VINPAT_NONE);
	zassert_false(vinpat_on_case(&v), "no VIN: not fed by anything");
	zassert_equal(vinpat_edge(&v, true, 20200), VINPAT_NONE);
	zassert_true(vinpat_on_case(&v), "one pulse keeps it");

	zassert_equal(pulse(30000, 5000), VINPAT_NONE);
	zassert_false(vinpat_on_case(&v), "a lift, or the case let go");
}

ZTEST(vinpat, test_bounce_is_ignored)
{
	zassert_equal(pulse(10000, 200), VINPAT_NONE);
	for (uint32_t t = 10300; t < 10400; t += 20) {
		zassert_equal(pulse(t, 5), VINPAT_NONE);
	}
	zassert_equal(pulse(10500, 200), VINPAT_CASE, "bounce neither counts nor clears");
}

ZTEST(vinpat, test_limits_of_a_pulse)
{
	zassert_equal(pulse(10000, 99), VINPAT_NONE);
	zassert_equal(pulse(10500, 99), VINPAT_NONE, "too short twice: nothing");
	zassert_equal(pulse(11000, 100), VINPAT_NONE);
	zassert_equal(pulse(11500, 400), VINPAT_CASE, "100 and 400 ms both count");
	zassert_equal(pulse(12500, 401), VINPAT_NONE);
	zassert_false(vinpat_on_case(&v), "401 ms is a long drop");
}

ZTEST(vinpat, test_long_drop_clears_the_count)
{
	zassert_equal(pulse(10000, 200), VINPAT_NONE);
	zassert_equal(pulse(10300, 600), VINPAT_NONE);
	zassert_equal(pulse(11000, 200), VINPAT_NONE);
	zassert_equal(pulse(11500, 200), VINPAT_CASE, "no reset across a lift");
}

ZTEST(vinpat, test_slow_pulses_do_not_reset)
{
	zassert_equal(pulse(10000, 200), VINPAT_NONE);
	zassert_equal(pulse(11500, 200), VINPAT_CASE);
	zassert_equal(pulse(13000, 200), VINPAT_CASE, "the first is out of the 2 s window");
	zassert_equal(pulse(15000, 200), VINPAT_NONE, "both earlier ones are out");
}

ZTEST(vinpat, test_signature_then_reset_spaced_as_the_case_does)
{
	/* signature at 3 s after VIN on; the pattern 2.5 s after it ends */
	zassert_equal(pulse(13000, 200), VINPAT_NONE);
	zassert_equal(pulse(13500, 200), VINPAT_CASE);
	zassert_equal(pulse(16200, 200), VINPAT_NONE, "not counted with the signature");
	zassert_equal(pulse(16700, 200), VINPAT_CASE);
	zassert_equal(pulse(17200, 200), VINPAT_RESET);
}

ZTEST(vinpat, test_power_up_and_repeated_levels)
{
	static const struct vinpat_cfg cfg = VINPAT_CFG_DEFAULT;

	vinpat_init(&v, &cfg, false);
	zassert_equal(vinpat_edge(&v, true, 5), VINPAT_NONE, "VIN at power-up");
	zassert_equal(vinpat_edge(&v, true, 10), VINPAT_NONE);
	zassert_equal(vinpat_edge(&v, false, 1000), VINPAT_NONE);
	zassert_equal(vinpat_edge(&v, false, 1100), VINPAT_NONE, "same level twice");
	zassert_equal(vinpat_edge(&v, true, 1200), VINPAT_NONE, "timed from the first edge");
	zassert_equal(pulse(1500, 200), VINPAT_CASE);
}
