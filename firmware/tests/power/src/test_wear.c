/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The cases of app/python/tests/test_wear.py on the port, with the firmware's own part:
 * one on-skin reference per LED, learned while worn with a pulse, cleared on the dock and
 * at off-body.
 */
#include <zephyr/ztest.h>

#include "modes.h"
#include "wear.h"

static struct wear w;

static void before(void *f)
{
	ARG_UNUSED(f);
	struct wear_cfg cfg = WEAR_CFG_DEFAULT;

	wear_init(&w, &cfg);
}

ZTEST_SUITE(wear, NULL, NULL, before, NULL, NULL);

#define SKIN 10000.0f /* on-skin level, counts per mA */
#define AIR  500.0f   /* 0.05 of it */

static bool run_led(int secs, uint8_t led, float dc, bool pulse, float mot, float temp)
{
	bool r = w.worn;

	for (int i = 0; i < secs; i++) {
		r = wear_update(&w, led, true, dc, pulse, mot, true, temp);
	}
	return r;
}

static bool run(int secs, float dc, bool pulse, float mot, float temp)
{
	return run_led(secs, LED_GREEN, dc, pulse, mot, temp);
}

/* the first pulse-present minute while worn sets the green reference */
static void learn_green(void)
{
	zassert_true(run(60, SKIN, true, 5, 34.0f));
	zassert_true(wear_has_ref(&w, LED_GREEN));
}

ZTEST(wear, test_starts_worn_and_stays)
{
	zassert_false(wear_has_ref(&w, LED_GREEN));
	zassert_true(run(600, SKIN, true, 5, 34.0f));
}

ZTEST(wear, test_learns_on_skin_reference)
{
	/* a darker skin: 2500 counts/mA */
	zassert_true(run(59, 2500.0f, true, 5, 34.0f));
	zassert_false(wear_has_ref(&w, LED_GREEN), "59 s are not a minute");
	zassert_true(run(1, 2500.0f, true, 5, 34.0f));
	zassert_within(w.dc_ref[LED_GREEN], 2500.0f, 1.0f);
	zassert_false(run(4, 100.0f, false, 5, 34.0f), "air is dark against the learned level");
}

ZTEST(wear, test_lifted_into_air_goes_dark)
{
	learn_green();
	zassert_true(run(3, AIR, false, 30, 34.0f), "3 s dark is not enough");
	zassert_false(run(1, AIR, false, 30, 34.0f));
	zassert_false(wear_has_ref(&w, LED_GREEN), "off-body clears the reference");
}

/* wear.py test_no_reference_uses_pulse_and_temperature_only */
ZTEST(wear, test_no_reference_uses_pulse_and_temperature_only)
{
	/* dark, warm and moving with no reference: the dark path is skipped */
	zassert_true(run(120, AIR, false, 30, 34.0f));
	/* still and cool: the no-pulse path takes it off after 60 s without a pulse */
	before(NULL);
	zassert_true(run(59, AIR, false, 2, 26.0f));
	zassert_false(run(1, AIR, false, 2, 26.0f));
	/* back on: a pulse alone, even in the dark, no lit and confirm step */
	zassert_true(run(1, AIR, true, 5, 33.0f));
}

ZTEST(wear, test_exercise_masks_pulse_but_stays_worn)
{
	learn_green();
	/* walking: no pulse presence (the step artefact), moving, warm and lit */
	zassert_true(run(900, SKIN, false, 80, 34.5f));
}

ZTEST(wear, test_on_a_table_still_and_cool)
{
	learn_green();
	zassert_true(run(59, SKIN, false, 2, 26.0f));
	zassert_false(run(1, SKIN, false, 2, 26.0f));
}

ZTEST(wear, test_cooling_fast_counts_as_cool)
{
	/* 33 C but falling 0.5 C a minute, still, no pulse */
	bool r = true;

	for (int i = 0; i < 60; i++) {
		r = wear_update(&w, LED_GREEN, true, SKIN, false, 2, true, 33.0f - i * 0.5f / 60.0f);
	}
	zassert_false(r);
}

ZTEST(wear, test_pocket_long_no_pulse_not_warm)
{
	/* moving cloth reflects, no pulse, 31 C: off after 300 s */
	zassert_true(run(299, SKIN, false, 60, 31.0f));
	zassert_false(run(1, SKIN, false, 60, 31.0f));
}

ZTEST(wear, test_back_on_with_a_pulse_after_off_body)
{
	learn_green();
	run(4, AIR, false, 5, 30.0f);
	zassert_false(w.worn);
	zassert_false(run(5, SKIN, false, 5, 33.0f), "lit, but no pulse yet");
	zassert_true(run(1, SKIN, true, 5, 33.0f));
	/* and the reference is learned again in the new wear's first pulse minute */
	zassert_false(wear_has_ref(&w, LED_GREEN));
	run(60, SKIN, true, 5, 34.0f);
	zassert_true(wear_has_ref(&w, LED_GREEN));
}

/* Low battery: green swaps to IR. The worn state carries over, IR has no reference yet, so
 * the IR level cannot take the pod off until IR has its own minute. */
ZTEST(wear, test_swap_to_ir_keeps_worn_and_learns_ir)
{
	learn_green();
	/* IR reads 0.1 of green's counts per mA: against green's reference that is dark */
	zassert_true(run_led(30, LED_IR, 0.1f * SKIN, false, 30, 34.0f), "no IR reference: not dark");
	zassert_false(wear_has_ref(&w, LED_IR));
	zassert_true(run_led(60, LED_IR, 0.1f * SKIN, true, 5, 34.0f));
	zassert_true(wear_has_ref(&w, LED_IR));
	zassert_within(w.dc_ref[LED_IR], 0.1f * SKIN, 1.0f);
	zassert_true(wear_has_ref(&w, LED_GREEN), "green keeps its own");
	/* IR into the air: dark against the IR reference */
	zassert_false(run_led(4, LED_IR, 0.01f * SKIN, false, 30, 34.0f));
}

ZTEST(wear, test_swap_mid_minute_restarts_learning)
{
	run(30, SKIN, true, 5, 34.0f);
	run_led(30, LED_IR, 2000.0f, true, 5, 34.0f);
	zassert_false(wear_has_ref(&w, LED_GREEN));
	zassert_false(wear_has_ref(&w, LED_IR), "30 s on IR is not a minute");
	run_led(30, LED_IR, 2000.0f, true, 5, 34.0f);
	zassert_true(wear_has_ref(&w, LED_IR));
	zassert_within(w.dc_ref[LED_IR], 2000.0f, 1.0f, "only IR seconds in the IR mean");
}

ZTEST(wear, test_dock_clears_references)
{
	learn_green();
	wear_restart(&w);
	zassert_true(w.worn);
	zassert_false(wear_has_ref(&w, LED_GREEN));
	zassert_false(wear_has_ref(&w, LED_IR));
}

ZTEST(wear, test_no_sample_holds_state)
{
	learn_green();
	run(4, AIR, false, 5, 30.0f);
	for (int i = 0; i < 10; i++) {
		zassert_false(wear_update(&w, LED_GREEN, false, SKIN, true, 5, true, 33.0f));
	}
}
