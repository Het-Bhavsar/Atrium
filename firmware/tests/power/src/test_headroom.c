/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "headroom.h"
#include "modes.h"

ZTEST_SUITE(headroom, NULL, NULL, NULL, NULL, NULL);

ZTEST(headroom, test_limit_adds_pulse_sag)
{
	/* 3.5 V green need, 15 mA green, 2.0 ohm cell at end of life and cold [ASSUMED] */
	zassert_equal(headroom_limit_mv(3500, 15, 2000), 3530);
	zassert_equal(headroom_limit_mv(3500, 0, 2000), 3500);
}

ZTEST(headroom, test_green_drops_and_returns_with_hysteresis)
{
	struct headroom h;

	headroom_init(&h, 3530);
	zassert_true(headroom_update(&h, 3800));
	zassert_true(headroom_update(&h, 3530));
	zassert_false(headroom_update(&h, 3529));
	zassert_false(headroom_update(&h, 3570), "inside the hysteresis");
	zassert_true(headroom_update(&h, 3580));
}

ZTEST(headroom, test_green_swaps_to_ir)
{
	struct mode_cfg low;

	for (int i = 0; i < MODE_COUNT; i++) {
		const struct mode_cfg *m = mode_get(i);
		uint8_t n = mode_green_to_ir(m, &low);
		bool had_green = false;

		for (uint8_t s = 0; s < m->n_slots; s++) {
			had_green |= m->slot[s].led == LED_GREEN;
		}
		for (uint8_t s = 0; s < n; s++) {
			zassert_not_equal(low.slot[s].led, LED_GREEN, "%s", m->name);
		}
		/* every mode with green keeps a pulse source, and never repeats an LED */
		if (had_green) {
			zassert_true(n > 0, "%s has no pulse source", m->name);
		}
		uint8_t used = 0;

		for (uint8_t s = 0; s < n; s++) {
			zassert_false(used & BIT(low.slot[s].led), "%s repeats an LED", m->name);
			used |= BIT(low.slot[s].led);
		}
	}
	/* smart's single green slot becomes IR at the same current */
	mode_green_to_ir(mode_get(MODE_SMART), &low);
	zassert_equal(low.n_slots, 1);
	zassert_equal(low.slot[0].led, LED_IR);
	zassert_equal(low.slot[0].ma, 15);
	/* full already has IR: green is dropped */
	zassert_equal(mode_green_to_ir(mode_get(MODE_FULL), &low), 2);
}
