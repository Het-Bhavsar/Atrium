/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "charge_policy.h"

static struct charge_policy p;
static uint32_t now;

static void before(void *f)
{
	ARG_UNUSED(f);
	struct charge_policy_cfg cfg = CHARGE_POLICY_CFG_DEFAULT;

	charge_policy_init(&p, &cfg);
	now = 1000;
}

ZTEST_SUITE(charge_policy, NULL, NULL, before, NULL, NULL);

static struct charge_temps both(int32_t tmp, int32_t ntc)
{
	struct charge_temps t = {
		.primary = {.valid = true, .cc = tmp, .at_ms = now},
		.backup = {.valid = true, .cc = ntc, .at_ms = now},
	};
	return t;
}

static bool step(int32_t tmp, int32_t ntc)
{
	struct charge_temps t = both(tmp, ntc);

	return charge_policy_update(&p, now, true, &t);
}

/* Average charge current over `secs` at a fixed temperature, worst-case charger current. */
static uint32_t avg_ua(int32_t cc, uint32_t secs)
{
	uint64_t on = 0;

	for (uint32_t s = 0; s < secs; s++) {
		now += 1000;
		if (!step(cc, cc)) {
			on++;
		}
	}
	return (uint32_t)(on * p.cfg.chg_cold_ua / secs);
}

ZTEST(charge_policy, test_cold_on_time)
{
	/* Rev B.2: 0.2C of 30 mAh = 6 mA is above the 5.451 mA half current: no duty cycle */
	zassert_equal(charge_policy_cold_on_ms(&p.cfg), 60000);
	/* a 20 mAh cell on the same charger: 60 s * 4 / 5.451 = 44.03 s */
	p.cfg.cell_mah = 20;
	zassert_equal(charge_policy_cold_on_ms(&p.cfg), 44028);
}

ZTEST(charge_policy, test_normal_band_charges)
{
	zassert_false(step(2500, 2500));
	zassert_equal(p.band, CHG_BAND_NORMAL);
}

ZTEST(charge_policy, test_hot_stops_with_hysteresis)
{
	zassert_false(step(4500, 4500));     /* 45.00 C is still inside */
	zassert_true(step(4501, 4501));
	zassert_equal(p.band, CHG_BAND_HOT);
	zassert_true(step(4450, 4450));      /* inside the 1 C hysteresis */
	zassert_true(step(4401, 4401));
	zassert_false(step(4400, 4400));
}

ZTEST(charge_policy, test_freeze_stops_with_hysteresis)
{
	zassert_true(step(-1, -1));
	zassert_equal(p.band, CHG_BAND_FREEZE);
	zassert_true(step(50, 50));
	zassert_false(step(100, 100));       /* 1.0 C: cold band, on phase */
	zassert_equal(p.band, CHG_BAND_COLD);
}

ZTEST(charge_policy, test_cold_band_edges)
{
	zassert_false(step(1000, 1000));
	zassert_equal(p.band, CHG_BAND_NORMAL);
	step(999, 999);
	zassert_equal(p.band, CHG_BAND_COLD);
	step(1050, 1050);
	zassert_equal(p.band, CHG_BAND_COLD);
	step(1100, 1100);
	zassert_equal(p.band, CHG_BAND_NORMAL);
}

ZTEST(charge_policy, test_cold_average_at_most_0p2c)
{
	uint32_t limit = p.cfg.cell_mah * p.cfg.cold_rate_milli_c; /* 6000 uA */
	/* on 30 mAh the 5451 uA half current is under the cap, so it charges unbroken */
	uint32_t want = MIN(limit, p.cfg.chg_cold_ua);
	uint32_t a = avg_ua(500, 3600);

	zassert_true(a <= limit, "average %u uA", a);
	zassert_true(a >= want * 95 / 100, "too conservative: %u uA", a);
}

ZTEST(charge_policy, test_primary_decides)
{
	/* step(primary TH4, backup TH3): the primary is over the limit, the backup is not */
	zassert_true(step(4600, 4400));
	zassert_equal(p.band, CHG_BAND_HOT);
	/* backup over, primary inside: the primary decides */
	charge_policy_init(&p, &p.cfg);
	zassert_false(step(4400, 4600));
	zassert_equal(p.band, CHG_BAND_NORMAL);
	step(900, 1200);
	zassert_equal(p.band, CHG_BAND_COLD);
}

ZTEST(charge_policy, test_backup_takes_over)
{
	struct charge_temps t = both(2500, 4800);

	t.primary.valid = false; /* TH4 open: TH3 decides */
	zassert_true(charge_policy_update(&p, now, true, &t));
	zassert_equal(p.band, CHG_BAND_HOT);
}

/* r5: TH4 alone, the backup slot never valid. TH4 decides every band, and losing it stops charging. */
ZTEST(charge_policy, test_primary_alone)
{
	static const struct {
		int32_t cc;
		bool inhibit;
	} pts[] = {{2500, false}, {4600, true}, {-100, true}};

	for (size_t i = 0; i < ARRAY_SIZE(pts); i++) {
		struct charge_policy_cfg cfg = CHARGE_POLICY_CFG_DEFAULT;
		struct charge_temps t = {.primary = {.valid = true, .cc = pts[i].cc, .at_ms = now}};

		charge_policy_init(&p, &cfg);
		zassert_equal(charge_policy_update(&p, now, true, &t), pts[i].inhibit, "%d cC", pts[i].cc);
	}
	struct charge_temps t = {.primary = {.valid = true, .cc = 2500, .at_ms = now}};

	zassert_false(charge_policy_update(&p, now, true, &t));
	now += 11000; /* TH4 goes stale */
	zassert_true(charge_policy_update(&p, now, true, &t), "no fresh reading: inhibit");
	t.primary.valid = false;
	t.primary.at_ms = now;
	zassert_true(charge_policy_update(&p, now, true, &t), "TH4 open: inhibit");
}

ZTEST(charge_policy, test_one_stale_sensor_uses_other)
{
	struct charge_temps t = both(2500, 4800);

	t.backup.at_ms = now - 20000; /* stale and hot backup: ignored */
	zassert_false(charge_policy_update(&p, now, true, &t));
	zassert_equal(p.band, CHG_BAND_NORMAL);
}

ZTEST(charge_policy, test_no_temperature_inhibits)
{
	struct charge_temps t = {0};

	zassert_true(charge_policy_update(&p, now, true, &t));
	zassert_equal(p.band, CHG_BAND_FAULT);

	t = both(2500, 2500);
	now += 10001;
	zassert_true(charge_policy_update(&p, now, true, &t));
}

ZTEST(charge_policy, test_disagreement_is_a_fault)
{
	zassert_true(step(1500, 2600));
	zassert_equal(p.band, CHG_BAND_FAULT);
	zassert_false(step(1500, 2500));
}

ZTEST(charge_policy, test_undocked_releases_pin)
{
	struct charge_temps t = both(5000, 5000);

	zassert_false(charge_policy_update(&p, now, false, &t));
	zassert_equal(p.band, CHG_BAND_HOT);
}

/* logic_sim.py invariant "no charge outside 0-45 C, 0.2C max below 10 C", over a sweep. */
ZTEST(charge_policy, test_sweep_invariant)
{
	uint32_t limit = p.cfg.cell_mah * p.cfg.cold_rate_milli_c;

	for (int32_t cc = -1000; cc <= 6000; cc += 25) {
		struct charge_policy_cfg cfg = p.cfg;

		charge_policy_init(&p, &cfg);
		uint32_t a = avg_ua(cc, 600);

		if (cc < 0 || cc > 4500) {
			zassert_equal(a, 0, "charging at %d cC", cc);
		} else if (cc < 1000) {
			zassert_true(a <= limit, "%u uA at %d cC", a, cc);
		} else {
			zassert_equal(a, p.cfg.chg_cold_ua, "blocked at %d cC", cc);
		}
	}
}

/* The cold-band cap holds for any cell and charger the board devicetree sets. Worst-case ICHG is
 * K_ISET max 145 / R1: 13.3 kOhm on the 30 mAh B.2 cell (half current under the cap: always on),
 * the same charger on a 20 mAh cell, 9.09 kOhm on 30 mAh, and 13.5 kOhm (the BQ25101 maximum) on
 * a 10 mAh cell, where the cap is 2 mA. */
ZTEST(charge_policy, test_cold_cap_across_cells)
{
	static const struct {
		uint16_t mah;
		uint32_t ichg_max_ua;
	} cells[] = {{10, 10741}, {20, 10902}, {30, 10902}, {30, 15950}};

	for (size_t i = 0; i < ARRAY_SIZE(cells); i++) {
		struct charge_policy_cfg cfg = CHARGE_POLICY_CFG_DEFAULT;

		cfg.cell_mah = cells[i].mah;
		cfg.chg_cold_ua = cells[i].ichg_max_ua / 2;
		charge_policy_init(&p, &cfg);
		uint32_t limit = cfg.cell_mah * cfg.cold_rate_milli_c;
		uint32_t want = MIN(limit, cfg.chg_cold_ua); /* the charger cannot exceed its half current */
		uint32_t a = avg_ua(500, 3600);

		zassert_true(a <= limit, "%u mAh: %u uA over %u", cells[i].mah, a, limit);
		zassert_true(a >= want * 95 / 100, "%u mAh: too conservative, %u uA", cells[i].mah, a);
	}
}

ZTEST(charge_policy, test_band_names)
{
	zassert_str_equal(charge_band_name(CHG_BAND_COLD), "cold");
	zassert_str_equal(charge_band_name((enum charge_band)99), "?");
}
