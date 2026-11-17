/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * mlc.c against app/python/atrium_ml/mlc.py on the mlc_* vectors from make_test_vectors.py
 * (12.5 Hz): the tree thresholds, the ladder on each window's variance, the on-pod window
 * (mlc_win_*) against that, and the step rhythm and class per 3-window record. Tolerances [ASSUMED, from
 * the ML hand-over]: variance within 1 %, classes equal.
 */
#include <math.h>
#include <string.h>
#include <zephyr/ztest.h>

#include "mlc.h"
#include "vec_csv.h"

static const char still_in[] = {
#include "vectors/mlc_still_input.inc"
	0};
static const char still_lv[] = {
#include "vectors/mlc_still_levels.inc"
	0};
static const char still_cl[] = {
#include "vectors/mlc_still_class.inc"
	0};
static const char head_in[] = {
#include "vectors/mlc_head_motion_input.inc"
	0};
static const char head_lv[] = {
#include "vectors/mlc_head_motion_levels.inc"
	0};
static const char head_cl[] = {
#include "vectors/mlc_head_motion_class.inc"
	0};
static const char walk_in[] = {
#include "vectors/mlc_walk_input.inc"
	0};
static const char walk_lv[] = {
#include "vectors/mlc_walk_levels.inc"
	0};
static const char walk_cl[] = {
#include "vectors/mlc_walk_class.inc"
	0};
static const char run_in[] = {
#include "vectors/mlc_run_input.inc"
	0};
static const char run_lv[] = {
#include "vectors/mlc_run_levels.inc"
	0};
static const char run_cl[] = {
#include "vectors/mlc_run_class.inc"
	0};

#define MAX_IN 320

static float norm_g[MAX_IN];
static int n_in;

static void load_norm(const char *csv)
{
	const char *p = next_line(csv);
	double f[MAXF];

	n_in = 0;
	while (p && *p && n_in < MAX_IN) {
		if (fields(p, f) >= 4) {
			norm_g[n_in++] = sqrtf((float)(f[1] * f[1] + f[2] * f[2] + f[3] * f[3])) / 1000.0f;
		}
		p = next_line(p);
	}
}

static int class_of(const char *line)
{
	static const char *const names[] = {"still", "walk", "run", "other"};
	const char *c = line, *e;

	for (const char *q = line; *q && *q != '\n'; q++) {
		if (*q == ',') {
			c = q + 1;
		}
	}
	for (e = c; *e && *e != '\n' && *e != '\r'; e++) {
	}
	for (int k = 0; k < 4; k++) {
		if ((size_t)(e - c) == strlen(names[k]) && !strncmp(c, names[k], (size_t)(e - c))) {
			return k;
		}
	}
	return -1;
}

ZTEST_SUITE(mlc, NULL, NULL, NULL, NULL, NULL);

ZTEST(mlc, test_window_lengths)
{
	/* the reference's 39 and 100 samples at 12.5 Hz; the same seconds at 15 Hz */
	zassert_equal(mlc_window_len(12500), 39);
	zassert_equal(mlc_rhythm_len(12500), 100);
	zassert_equal(mlc_window_len(15000), 47);
	zassert_equal(mlc_rhythm_len(15000), 120);
	zassert_true(mlc_rhythm_len(120000) <= MLC_RHYTHM_MAX);
}

ZTEST(mlc, test_thresholds)
{
	static const float t[7] = {1e-4f, 1e-3f, 4e-3f, 1e-2f, 3e-2f, 8e-2f, 0.25f};

	for (int k = 0; k < 7; k++) {
		/* half precision: 11 significant bits */
		zassert_within((double)mlc_threshold(k), (double)t[k], (double)t[k] * 1e-3, "threshold %d", k);
	}
	zassert_equal(mlc_level_of(0.0f), 0);
	zassert_equal(mlc_level_of(mlc_threshold(0)), 0, "equal is not above (side=left)");
	zassert_equal(mlc_level_of(1.0f), 7);
}

static void check_case(const char *name, const char *in, const char *lv_csv, const char *cl_csv)
{
	const int win = mlc_window_len(12500), rhythm = mlc_rhythm_len(12500);
	uint8_t lv[16], on_pod[16];
	int n_lv = 0, n_pod = 0;
	double f[MAXF];
	struct mlc_win pw;

	load_norm(in);
	/* the pod's streaming window over the same samples */
	mlc_win_init(&pw, (uint8_t)win);
	for (int i = 0; i < n_in && n_pod < 16; i++) {
		if (mlc_win_push(&pw, norm_g[i], &on_pod[n_pod])) {
			n_pod++;
		}
	}
	for (const char *p = next_line(lv_csv); p && *p; p = next_line(p)) {
		zassert_true(fields(p, f) >= 3);
		int w = (int)f[0];
		double mean = 0, var = 0;

		for (int i = 0; i < win; i++) {
			mean += (double)norm_g[w * win + i];
		}
		mean /= win;
		for (int i = 0; i < win; i++) {
			double d = (double)norm_g[w * win + i] - mean;

			var += d * d;
		}
		var /= win;
		zassert_within(var, f[1], fabs(f[1]) * 0.01 + 1e-9, "%s window %d variance", name, w);
		lv[n_lv] = mlc_level_of((float)var);
		zassert_equal(lv[n_lv], (uint8_t)f[2], "%s window %d level", name, w);
		zassert_true(w < n_pod, "%s window %d on the pod", name, w);
		zassert_equal(on_pod[w], lv[n_lv], "%s window %d: the pod's level", name, w);
		n_lv++;
	}
	for (const char *p = next_line(cl_csv); p && *p; p = next_line(p)) {
		struct mlc_record r;
		float spm = NAN, share = 0.0f;

		zassert_true(fields(p, f) >= 3);
		int k = (int)f[0];
		int end = (k + 3) * win;

		mlc_record_reset(&r);
		for (int i = k; i < k + 3; i++) {
			mlc_record_add(&r, lv[i]);
		}
		bool ok = mlc_step_rhythm(&norm_g[end - rhythm], rhythm, 12.5f, &spm, &share);

		if (!isnan(f[1])) {
			zassert_true(ok, "%s record %d", name, k);
			zassert_within((double)spm, f[1], 0.05, "%s record %d spm %.2f", name, k, (double)spm);
			zassert_within((double)share, f[2], 0.002, "%s record %d share", name, k);
		}
		zassert_equal((int)mlc_activity_class(&r, ok, spm, share), class_of(p),
			      "%s record %d class", name, k);
	}
}

ZTEST(mlc, test_vectors)
{
	check_case("still", still_in, still_lv, still_cl);
	check_case("head_motion", head_in, head_lv, head_cl);
	check_case("walk", walk_in, walk_lv, walk_cl);
	check_case("run", run_in, run_lv, run_cl);
}

ZTEST(mlc, test_record_median_truncates)
{
	struct mlc_record r;

	mlc_record_reset(&r);
	zassert_equal(mlc_record_median(&r), 0);
	mlc_record_add(&r, 4);
	mlc_record_add(&r, 5);
	zassert_equal(mlc_record_median(&r), 4, "median 4.5 truncates to 4");
	mlc_record_add(&r, 7);
	zassert_equal(mlc_record_median(&r), 5);
	zassert_equal(mlc_record_max(&r), 7);
	/* a level-3 rhythm alone is never walking */
	mlc_record_reset(&r);
	mlc_record_add(&r, 3);
	zassert_equal(mlc_activity_class(&r, true, 105.0f, 0.9f), MLC_OTHER);
}
