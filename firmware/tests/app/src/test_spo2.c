/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * spo2.c against app/python/atrium_ml/spo2.py on the spo2_* vectors from
 * make_test_vectors.py: every window (R, correlation, IR perfusion, good) and the burst
 * result. Tolerances [ASSUMED, from the ML hand-over]: R within 0.5 %, flags equal.
 * Plus the burst schedule.
 */
#include <math.h>
#include <zephyr/ztest.h>

#include "spo2.h"
#include "vec_csv.h"

static const char r045_in[] = {
#include "vectors/spo2_r045_input.inc"
	0};
static const char r045_win[] = {
#include "vectors/spo2_r045_windows.inc"
	0};
static const char r045_burst[] = {
#include "vectors/spo2_r045_burst.inc"
	0};
static const char r070_in[] = {
#include "vectors/spo2_r070_input.inc"
	0};
static const char r070_win[] = {
#include "vectors/spo2_r070_windows.inc"
	0};
static const char r070_burst[] = {
#include "vectors/spo2_r070_burst.inc"
	0};
static const char moved_in[] = {
#include "vectors/spo2_moved_input.inc"
	0};
static const char moved_win[] = {
#include "vectors/spo2_moved_windows.inc"
	0};
static const char moved_burst[] = {
#include "vectors/spo2_moved_burst.inc"
	0};
static const char low_in[] = {
#include "vectors/spo2_low_perfusion_input.inc"
	0};
static const char low_win[] = {
#include "vectors/spo2_low_perfusion_windows.inc"
	0};
static const char low_burst[] = {
#include "vectors/spo2_low_perfusion_burst.inc"
	0};

static struct spo2 s;

ZTEST_SUITE(spo2, NULL, NULL, NULL, NULL, NULL);

static void run_case(const char *name, const char *in, const char *win, const char *burst)
{
	struct spo2_cfg cfg = SPO2_CFG_DEFAULT;
	struct spo2_out out;
	double f[MAXF];
	int k = 0;

	spo2_init(&s, &cfg);
	for (const char *p = next_line(in); p && *p; p = next_line(p), k++) {
		zassert_true(fields(p, f) >= 5);
		if (k % SPO2_FS == 0) {
			spo2_gate(&s, (float)f[3], f[4] != 0.0);
		}
		spo2_push(&s, (int32_t)f[1], (int32_t)f[2]);
	}
	zassert_equal(k, SPO2_SAMPLES, "%s: 30 s at 25 Hz", name);
	zassert_true(spo2_done(&s));

	int w = 0;

	for (const char *p = next_line(win); p && *p; p = next_line(p), w++) {
		zassert_true(fields(p, f) >= 5);
		zassert_true(w < s.n_win);
		zassert_within((double)s.win[w].t_s, f[0], 1e-3, "%s window %d time", name, w);
		if (isnan(f[1])) {
			zassert_true(isnan(s.win[w].r), "%s window %d R", name, w);
		} else {
			zassert_within((double)s.win[w].r, f[1], fabs(f[1]) * 0.005, "%s window %d R %.5f",
				       name, w, (double)s.win[w].r);
		}
		zassert_within((double)s.win[w].corr, f[2], 1e-3, "%s window %d corr", name, w);
		zassert_within((double)s.win[w].pi_ir, f[3], f[3] * 0.005 + 1e-6, "%s window %d PI", name, w);
		zassert_equal(s.win[w].good, f[4] != 0.0, "%s window %d good", name, w);
	}
	zassert_equal(w, SPO2_WINDOWS);
	zassert_equal(s.n_win, SPO2_WINDOWS);

	spo2_result(&s, &out);
	zassert_true(fields(next_line(burst), f) >= 4);
	if (isnan(f[0])) {
		zassert_true(isnan(out.r), "%s: no burst R", name);
	} else {
		zassert_within((double)out.r, f[0], f[0] * 0.005, "%s burst R %.5f", name, (double)out.r);
	}
	zassert_within((double)out.confidence, f[1], 0.01, "%s confidence %.4f", name,
		       (double)out.confidence);
	zassert_equal(out.n_good, (uint8_t)f[2], "%s good windows", name);
	zassert_equal(out.n_windows, (uint8_t)f[3], "%s windows", name);
}

ZTEST(spo2, test_vectors)
{
	run_case("r045", r045_in, r045_win, r045_burst);
	run_case("r070", r070_in, r070_win, r070_burst);
	run_case("moved", moved_in, moved_win, moved_burst);
	run_case("low_perfusion", low_in, low_win, low_burst);
}

ZTEST(spo2, test_ratio_follows_the_red_ir_ratio)
{
	/* the two clean cases differ only in the true R: the estimate must keep their order */
	struct spo2_out a, b;
	struct spo2_cfg cfg = SPO2_CFG_DEFAULT;
	double f[MAXF];
	const char *cases[2] = {r045_in, r070_in};
	struct spo2_out *outs[2] = {&a, &b};

	for (int c = 0; c < 2; c++) {
		spo2_init(&s, &cfg);
		for (const char *p = next_line(cases[c]); p && *p; p = next_line(p)) {
			fields(p, f);
			spo2_push(&s, (int32_t)f[1], (int32_t)f[2]);
		}
		spo2_result(&s, outs[c]);
	}
	zassert_within((double)a.r, 0.45, 0.02);
	zassert_within((double)b.r, 0.70, 0.02);
}

static uint32_t run_sched(struct spo2_sched *q, enum pod_mode mode, uint32_t from, uint32_t to,
			  uint32_t move_at, uint32_t *bursts, int max)
{
	int n = 0;

	for (uint32_t t = from; t < to; t++) {
		if (t % 3 == 0) {
			spo2_sched_level(q, t, t >= move_at && t < move_at + 3 ? 4 : 0);
		}
		if (spo2_sched_tick(q, t, mode, true, true) && n < max) {
			bursts[n++] = t;
		}
	}
	return (uint32_t)n;
}

ZTEST(spo2, test_schedule)
{
	struct spo2_sched q;
	uint32_t b[16];

	/* sleep: every 5 min; a move at 550 s pushes the 600 s burst to the next still minute */
	spo2_sched_init(&q, 0);
	zassert_equal(run_sched(&q, MODE_SLEEP, 0, 1000, 550, b, 16), 3);
	zassert_equal(b[0], 300);
	zassert_equal(b[1], 660);
	zassert_equal(b[2], 960);

	/* smart: every 30 min */
	spo2_sched_init(&q, 0);
	zassert_equal(run_sched(&q, MODE_SMART, 0, 4000, UINT32_MAX - 10, b, 16), 2);
	zassert_equal(b[0], 1800);
	zassert_equal(b[1], 3600);

	/* none in full, lite or off-body */
	static const enum pod_mode none[] = {MODE_FULL, MODE_LITE, MODE_OFFBODY, MODE_DOCKED_IDLE};

	for (size_t i = 0; i < ARRAY_SIZE(none); i++) {
		spo2_sched_init(&q, 0);
		zassert_equal(run_sched(&q, none[i], 0, 4000, UINT32_MAX - 10, b, 16), 0);
	}

	/* no MLC level for 10 s (IMU stopped): no burst */
	spo2_sched_init(&q, 0);
	spo2_sched_level(&q, 0, 0);
	for (uint32_t t = 0; t < 1000; t++) {
		zassert_false(spo2_sched_tick(&q, t, MODE_SLEEP, true, true));
	}

	/* no pulse, or the optics unavailable: no burst */
	spo2_sched_init(&q, 0);
	for (uint32_t t = 0; t < 1000; t++) {
		spo2_sched_level(&q, t, 0);
		zassert_false(spo2_sched_tick(&q, t, MODE_SLEEP, true, false));
		zassert_false(spo2_sched_tick(&q, t, MODE_SLEEP, false, true));
	}
}
