/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * The cases of app/python/tests/test_wear.py, on the port.
 */
#include <zephyr/ztest.h>

#include "wear.h"

static struct wear w;

static void before(void *f)
{
	ARG_UNUSED(f);
	struct wear_cfg cfg = WEAR_CFG_DEFAULT;

	wear_init(&w, &cfg);
}

ZTEST_SUITE(wear, NULL, NULL, before, NULL, NULL);

#define SKIN 10000.0f /* the default on-skin reference */
#define AIR  500.0f   /* 0.05 of it */

static bool run(int secs, float dc, bool pulse, float mot, float temp)
{
	bool r = w.worn;

	for (int i = 0; i < secs; i++) {
		r = wear_update(&w, true, dc, pulse, mot, true, temp);
	}
	return r;
}

ZTEST(wear, test_starts_worn_and_stays)
{
	zassert_true(run(600, SKIN, true, 5, 34.0f));
}

ZTEST(wear, test_lifted_into_air_goes_dark)
{
	zassert_true(run(3, AIR, false, 30, 34.0f), "3 s dark is not enough");
	zassert_false(run(1, AIR, false, 30, 34.0f));
}

ZTEST(wear, test_exercise_masks_pulse_but_stays_worn)
{
	/* walking: no pulse presence (the step artefact), moving, warm and lit */
	zassert_true(run(900, SKIN, false, 80, 34.5f));
}

ZTEST(wear, test_on_a_table_still_and_cool)
{
	zassert_true(run(59, SKIN, false, 2, 26.0f));
	zassert_false(run(1, SKIN, false, 2, 26.0f));
}

ZTEST(wear, test_cooling_fast_counts_as_cool)
{
	/* 33 C but falling 0.5 C a minute, still, no pulse */
	bool r = true;

	for (int i = 0; i < 60; i++) {
		r = wear_update(&w, true, SKIN, false, 2, true, 33.0f - i * 0.5f / 60.0f);
	}
	zassert_false(r);
}

ZTEST(wear, test_pocket_long_no_pulse_not_warm)
{
	/* moving cloth reflects, no pulse, 31 C: off after 300 s */
	zassert_true(run(299, SKIN, false, 60, 31.0f));
	zassert_false(run(1, SKIN, false, 60, 31.0f));
}

ZTEST(wear, test_put_back_on_needs_light_then_pulse)
{
	run(4, AIR, false, 5, 30.0f);
	zassert_false(w.worn);
	zassert_false(run(5, SKIN, false, 5, 33.0f), "lit, but no pulse yet");
	zassert_true(run(1, SKIN, true, 5, 33.0f));
}

ZTEST(wear, test_confirm_window_expires)
{
	run(4, AIR, false, 5, 30.0f);
	run(2, SKIN, false, 5, 33.0f);   /* lit for 2 s: waiting for a pulse */
	run(16, SKIN, false, 5, 33.0f);  /* 15 s pass with none */
	zassert_false(w.worn);
	run(1, AIR, false, 5, 33.0f);
	zassert_false(run(1, SKIN, true, 5, 33.0f), "lit again for only 1 s");
	zassert_true(run(1, SKIN, true, 5, 33.0f));
}

ZTEST(wear, test_learns_on_skin_reference)
{
	/* a darker skin: 2500 counts/mA on skin, which the default reference would call dark */
	zassert_true(run(60, 2500.0f, true, 5, 34.0f));
	zassert_within(w.dc_ref, 2500.0f, 1.0f);
	zassert_true(run(30, 2500.0f, true, 5, 34.0f));
	zassert_false(run(4, 100.0f, false, 5, 34.0f), "air is still dark against the learned level");
}

ZTEST(wear, test_no_sample_holds_state)
{
	run(4, AIR, false, 5, 30.0f);
	for (int i = 0; i < 10; i++) {
		zassert_false(wear_update(&w, false, SKIN, true, 5, true, 33.0f));
	}
}
