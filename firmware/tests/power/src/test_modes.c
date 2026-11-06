/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "modes.h"

ZTEST_SUITE(modes, NULL, NULL, NULL, NULL, NULL);

/* MAXM86161 PPG_SR rates with one pulse per sample */
static bool rate_ok(uint8_t hz)
{
	static const uint8_t ok[] = {8, 16, 25, 32, 50, 64, 84, 100, 128, 200};

	for (size_t i = 0; i < ARRAY_SIZE(ok); i++) {
		if (ok[i] == hz) {
			return true;
		}
	}
	return false;
}

ZTEST(modes, test_docked_idle_within_limit)
{
	const struct mode_cfg *m = mode_get(MODE_DOCKED_IDLE);

	zassert_true(m->budget_typ_ua <= DOCKED_IDLE_LIMIT_UA);
	zassert_true(m->budget_wc_ua <= DOCKED_IDLE_LIMIT_UA);
	zassert_equal(m->n_slots, 0, "no LEDs while docked");
	zassert_equal(m->imu_mhz, 0);
	/* the charge policy needs a temperature at least every 10 s */
	zassert_true(m->tmp_period_s <= 10);
}

ZTEST(modes, test_off_body_probe_is_cheap)
{
	const struct mode_cfg *m = mode_get(MODE_OFFBODY);

	zassert_equal(m->n_slots, 1);
	zassert_equal(m->slot[0].led, LED_GREEN, "the probe keeps a green DC for wear detection");
	zassert_true(m->budget_wc_ua < mode_get(MODE_LITE)->budget_typ_ua);
}

ZTEST(modes, test_every_mode_fits_the_maxm86161)
{
	for (int i = 0; i < MODE_COUNT; i++) {
		const struct mode_cfg *m = mode_get(i);
		uint8_t used = 0;

		zassert_not_null(m->name);
		zassert_true(m->n_slots <= MODE_MAX_SLOTS, "%s", m->name);
		zassert_true(m->budget_typ_ua <= m->budget_wc_ua, "%s", m->name);
		zassert_true(m->tmp_period_s >= 1 && m->tmp_period_s <= 10, "%s", m->name);
		if (m->n_slots) {
			zassert_true(rate_ok(m->hz), "%s: %u Hz is not a PPG_SR rate", m->name, m->hz);
		}
		for (int s = 0; s < m->n_slots; s++) {
			const struct led_slot *l = &m->slot[s];

			/* one PA register per LED: each LED once per sample */
			zassert_false(used & BIT(l->led), "%s repeats LED %u", m->name, l->led);
			used |= BIT(l->led);
			zassert_true(l->ma > 0 && l->ma <= 124, "%s slot %d", m->name, s);
			if (l->led == LED_GREEN) {
				zassert_true(l->ma <= GREEN_MAX_MA, "%s green %u mA", m->name, l->ma);
			}
		}
		/* the raw stream is 3 bytes per sample */
		if (m->stream_bps) {
			zassert_equal(m->stream_bps, m->n_slots * m->hz * 3, "%s", m->name);
		}
	}
}

ZTEST(modes, test_ml_contract_rates)
{
	/* ML data contract: green at 25 Hz in every worn mode (best tested for HR and HRV),
	 * accelerometer at 12.5 Hz */
	for (int i = MODE_FULL; i <= MODE_SLEEP; i++) {
		const struct mode_cfg *m = mode_get(i);

		zassert_equal(m->hz, 25, "%s", m->name);
		zassert_equal(m->slot[0].led, LED_GREEN, "%s: pulse source is the first slot", m->name);
		zassert_equal(m->imu_mhz, 12500, "%s", m->name);
	}
}

ZTEST(modes, test_lite_duty_cycle)
{
	const struct mode_cfg *m = mode_get(MODE_LITE);
	int on = 0;

	for (uint32_t t = 0; t < 1200; t++) {
		on += mode_optics_on(m, t);
	}
	zassert_equal(on, 300, "30 s in every 120 s");
	zassert_true(mode_optics_on(mode_get(MODE_SMART), 77));
	zassert_false(mode_optics_on(mode_get(MODE_DOCKED_IDLE), 0));
}

ZTEST(modes, test_budget_order_and_values)
{
	zassert_equal(mode_get(MODE_SMART)->budget_typ_ua, 215);
	zassert_true(mode_get(MODE_FULL)->budget_typ_ua > mode_get(MODE_SMART)->budget_typ_ua);
	zassert_true(mode_get(MODE_SMART)->budget_typ_ua > mode_get(MODE_LITE)->budget_typ_ua);
}

ZTEST(modes, test_family_fallbacks)
{
	zassert_equal(mode_sleep_of(MODE_SMART), MODE_SLEEP);
	zassert_equal(mode_lite_of(MODE_FULL), MODE_LITE);
	zassert_equal(mode_lite_of(MODE_SMART), MODE_LITE);
	zassert_equal(mode_lite_of(MODE_SLEEP), MODE_SLEEP);
	zassert_is_null(mode_get(MODE_COUNT));
}
