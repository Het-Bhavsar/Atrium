/*
 * Copyright (c) 2026 Het Bhavsar
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
			/* the module's own rate is a PPG_SR rate; with SMP_AVE the FIFO runs slower */
			zassert_true(mode_sample_hz(m) <= 255 && rate_ok((uint8_t)mode_sample_hz(m)),
				     "%s: %u Hz is not a PPG_SR rate", m->name, mode_sample_hz(m));
			zassert_equal(mode_sample_hz(m) % mode_avg(m), 0, "%s", m->name);
		}
		for (int s = 0; s < m->n_slots; s++) {
			const struct led_slot *l = &m->slot[s];

			/* one PA register per LED: each LED once per sample */
			zassert_false(used & BIT(l->led), "%s repeats LED %u", m->name, l->led);
			used |= BIT(l->led);
			if (l->led == LED_AMBIENT) {
				/* direct ambient: no LED, and only the research modes sample it */
				zassert_equal(l->ma, 0, "%s slot %d", m->name, s);
				zassert_true(mode_is_research(i), "%s samples ambient", m->name);
				continue;
			}
			zassert_true(l->ma > 0 && l->ma <= 124, "%s slot %d", m->name, s);
			if (l->led == LED_GREEN) {
				zassert_true(l->ma <= GREEN_MAX_MA, "%s green %u mA", m->name, l->ma);
			}
		}
		/* the raw stream is 3 bytes per PPG sample; 2 per accelerometer sample in modes 10, 11 */
		if (mode_is_vibration(i)) {
			zassert_equal(m->stream_bps, (m->imu_mhz * 2 + 500) / 1000, "%s", m->name);
		} else if (m->stream_bps) {
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

ZTEST(modes, test_research_slot_table_and_stream)
{
	static const struct {
		enum pod_mode mode;
		uint8_t hz;
		uint16_t bps;
	} r[] = {{MODE_RESEARCH_100, 100, 1200}, {MODE_RESEARCH_200, 200, 2400}};

	for (size_t i = 0; i < ARRAY_SIZE(r); i++) {
		const struct mode_cfg *m = mode_get(r[i].mode);

		zassert_true(mode_is_research(r[i].mode));
		zassert_true(mode_user_selectable(r[i].mode), "%s", m->name);
		/* green, IR, red at full's currents, then direct ambient: tags 1 to 4 in the stream */
		zassert_equal(m->n_slots, 4, "%s", m->name);
		zassert_equal(mode_stream_tag(m, LED_GREEN), 1, "%s", m->name);
		zassert_equal(mode_stream_tag(m, LED_IR), 2, "%s", m->name);
		zassert_equal(mode_stream_tag(m, LED_RED), 3, "%s", m->name);
		zassert_equal(mode_stream_tag(m, LED_AMBIENT), 4, "%s", m->name);
		for (uint8_t s = 0; s < 3; s++) {
			zassert_equal(m->slot[s].ma, mode_get(MODE_FULL)->slot[s].ma, "%s", m->name);
		}
		zassert_equal(m->hz, r[i].hz, "%s", m->name);
		zassert_equal(m->imu_mhz, 104000, "%s: LSM6DSOX at 104 Hz", m->name);
		zassert_equal(m->optics_period_s, 0, "%s: no duty cycle", m->name);
		/* 3-byte FIFO words, 4 per sample */
		zassert_equal(m->stream_bps, r[i].bps, "%s", m->name);
		zassert_equal(m->stream_bps, 3 * 4 * m->hz, "%s", m->name);
		/* the 25 Hz filters get whole means of 4 or 8; the rhythm about 12.5 Hz */
		zassert_equal(mode_algo_decim(m) * MODE_ALGO_HZ, m->hz, "%s", m->name);
		zassert_equal(mode_imu_decim(m), 8, "%s", m->name);
		/* the only modes above 1 mA, and the green current sets no new headroom limit */
		zassert_true(m->budget_typ_ua > mode_get(MODE_FULL)->budget_wc_ua, "%s", m->name);
		zassert_true(m->ci_ms <= 30, "%s: the stream needs a short interval", m->name);
	}
	zassert_equal(mode_get(MODE_RESEARCH_100)->budget_typ_ua, 1512);
	zassert_equal(mode_get(MODE_RESEARCH_200)->budget_typ_ua, 2706);
	zassert_true(mode_get(MODE_RESEARCH_200)->budget_wc_ua >
		     mode_get(MODE_RESEARCH_100)->budget_wc_ua);
	/* never the default, never the docked or probe modes */
	zassert_false(mode_is_research(MODE_SMART));
	zassert_false(mode_user_selectable(MODE_DOCKED_IDLE));
	zassert_false(mode_user_selectable(MODE_OFFBODY));
	zassert_false(mode_user_selectable(MODE_COUNT));
	/* the 25 Hz modes are untouched */
	zassert_equal(mode_algo_decim(mode_get(MODE_SMART)), 1);
	zassert_equal(mode_imu_decim(mode_get(MODE_SMART)), 1);
	zassert_equal(mode_algo_decim(mode_get(MODE_OFFBODY)), 1);
	zassert_equal(mode_stream_tag(mode_get(MODE_SMART), LED_AMBIENT), 0);
	/* night and low battery leave research */
	zassert_equal(mode_sleep_of(MODE_RESEARCH_200), MODE_SLEEP);
	zassert_equal(mode_lite_of(MODE_RESEARCH_100), MODE_LITE);
}

ZTEST(modes, test_averaged_research_modes)
{
	static const struct {
		enum pod_mode mode;
		uint8_t out_hz, avg;
		uint16_t bps;
	} r[] = {{MODE_RESEARCH_AVG50, 50, 4, 600}, {MODE_RESEARCH_AVG25, 25, 8, 300}};
	const struct mode_cfg *m7 = mode_get(MODE_RESEARCH_200);

	for (size_t i = 0; i < ARRAY_SIZE(r); i++) {
		const struct mode_cfg *m = mode_get(r[i].mode);

		zassert_true(mode_is_research(r[i].mode), "%s", m->name);
		zassert_true(mode_user_selectable(r[i].mode), "%s", m->name);
		/* mode 7's slot table and module rate, averaged on the chip */
		zassert_equal(m->n_slots, m7->n_slots, "%s", m->name);
		for (uint8_t s = 0; s < m->n_slots; s++) {
			zassert_equal(m->slot[s].led, m7->slot[s].led, "%s slot %u", m->name, s);
			zassert_equal(m->slot[s].ma, m7->slot[s].ma, "%s slot %u", m->name, s);
		}
		zassert_equal(mode_stream_tag(m, LED_AMBIENT), 4, "%s", m->name);
		zassert_equal(mode_sample_hz(m), 200, "%s samples at 200 Hz", m->name);
		zassert_equal(mode_avg(m), r[i].avg, "%s", m->name);
		/* the header's rate byte and the stream are the output rate */
		zassert_equal(m->hz, r[i].out_hz, "%s", m->name);
		zassert_equal(m->stream_bps, r[i].bps, "%s", m->name);
		zassert_equal(m->stream_bps * r[i].avg, m7->stream_bps, "%s: 1/avg of mode 7", m->name);
		zassert_equal(mode_algo_decim(m) * MODE_ALGO_HZ, m->hz, "%s", m->name);
		zassert_equal(m->imu_mhz, 104000, "%s", m->name);
		/* the same exposures as mode 7, so about its optical current; less radio */
		zassert_true(m->budget_typ_ua < m7->budget_typ_ua, "%s", m->name);
		zassert_true(m->budget_typ_ua > 2000, "%s", m->name);
		zassert_true(m->budget_typ_ua <= m->budget_wc_ua, "%s", m->name);
	}
	zassert_equal(mode_get(MODE_RESEARCH_AVG50)->budget_typ_ua, 2443);
	zassert_equal(mode_get(MODE_RESEARCH_AVG25)->budget_typ_ua, 2408);
	/* point-sampled modes average nothing */
	zassert_equal(mode_avg(mode_get(MODE_FULL)), 1);
	zassert_equal(mode_avg(m7), 1);
	zassert_equal(mode_sample_hz(mode_get(MODE_SMART)), 25);
	zassert_equal(mode_lite_of(MODE_RESEARCH_AVG25), MODE_LITE);
}

ZTEST(modes, test_vibration_modes)
{
	static const struct {
		enum pod_mode mode;
		uint32_t imu_mhz;
		uint16_t typ;
	} v[] = {{MODE_RESEARCH_VIB3K3, 3333333, 1235}, {MODE_RESEARCH_VIB6K7, 6666667, 1898}};

	zassert_equal(MODE_RESEARCH_VIB3K3, 10, "the GATT value");
	zassert_equal(MODE_RESEARCH_VIB6K7, 11);
	for (size_t i = 0; i < ARRAY_SIZE(v); i++) {
		const struct mode_cfg *m = mode_get(v[i].mode);

		zassert_true(mode_is_vibration(v[i].mode));
		zassert_true(mode_is_research(v[i].mode), "%s", m->name);
		zassert_true(mode_user_selectable(v[i].mode), "%s", m->name);
		/* no LEDs, no PPG: the radio and the current are the accelerometer's */
		zassert_equal(m->n_slots, 0, "%s", m->name);
		zassert_equal(m->hz, 0, "%s", m->name);
		zassert_equal(m->imu_mhz, v[i].imu_mhz, "%s", m->name);
		zassert_equal(m->ci_ms, 15, "%s: the shortest iOS interval", m->name);
		zassert_equal(m->budget_typ_ua, v[i].typ, "%s", m->name);
		zassert_true(m->budget_typ_ua > mode_get(MODE_FULL)->budget_wc_ua, "%s", m->name);
		zassert_false(mode_optics_on(m, 0), "%s", m->name);
		zassert_equal(mode_lite_of(v[i].mode), MODE_LITE);
		zassert_equal(mode_sleep_of(v[i].mode), MODE_SLEEP);
		zassert_equal(mode_imu_decim(m), 255, "%s: saturates", m->name);
	}
	zassert_true(mode_get(MODE_RESEARCH_VIB6K7)->budget_wc_ua >
		     mode_get(MODE_RESEARCH_VIB3K3)->budget_wc_ua);
	for (int i = 0; i < MODE_COUNT; i++) {
		zassert_equal(mode_is_vibration(i),
			      i == MODE_RESEARCH_VIB3K3 || i == MODE_RESEARCH_VIB6K7);
	}
}
