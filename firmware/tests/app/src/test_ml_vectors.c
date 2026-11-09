/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The C ports of pulse.c (spectral HR and pulse presence) and hrv.c (max-slope beats,
 * interval cleaning, RMSSD) against the reference in app/python/atrium_ml, on the vectors
 * from app/python/make_test_vectors.py. Tolerances [ASSUMED, from the ML hand-over]: bpm
 * within 0.5, beat times within 2 ms, RMSSD within 1 ms, flags equal except within 2 s of a
 * change in the reference.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/ztest.h>

#include "hrv.h"
#include "pulse.h"

static const char rest_in[] = {
#include "vectors/rest_64bpm_input.inc"
	0};
static const char rest_hr[] = {
#include "vectors/rest_64bpm_hr.inc"
	0};
static const char rest_hrv[] = {
#include "vectors/rest_64bpm_hrv.inc"
	0};
static const char rest_rm[] = {
#include "vectors/rest_64bpm_rmssd.inc"
	0};
static const char walk_in[] = {
#include "vectors/walk_72bpm_steps108_input.inc"
	0};
static const char walk_hr[] = {
#include "vectors/walk_72bpm_steps108_hr.inc"
	0};
static const char walk_hrv[] = {
#include "vectors/walk_72bpm_steps108_hrv.inc"
	0};
static const char walk_rm[] = {
#include "vectors/walk_72bpm_steps108_rmssd.inc"
	0};
static const char run_in[] = {
#include "vectors/run_140bpm_steps168_input.inc"
	0};
static const char run_hr[] = {
#include "vectors/run_140bpm_steps168_hr.inc"
	0};
static const char run_hrv[] = {
#include "vectors/run_140bpm_steps168_hrv.inc"
	0};
static const char run_rm[] = {
#include "vectors/run_140bpm_steps168_rmssd.inc"
	0};
static const char hrv_in[] = {
#include "vectors/hrv_rest_58bpm_rmssd45_input.inc"
	0};
static const char hrv_hr[] = {
#include "vectors/hrv_rest_58bpm_rmssd45_hr.inc"
	0};
static const char hrv_hrv[] = {
#include "vectors/hrv_rest_58bpm_rmssd45_hrv.inc"
	0};
static const char hrv_rm[] = {
#include "vectors/hrv_rest_58bpm_rmssd45_rmssd.inc"
	0};
static const char off_in[] = {
#include "vectors/offbody_noise_input.inc"
	0};
static const char off_hr[] = {
#include "vectors/offbody_noise_hr.inc"
	0};
static const char off_hrv[] = {
#include "vectors/offbody_noise_hrv.inc"
	0};
static const char off_rm[] = {
#include "vectors/offbody_noise_rmssd.inc"
	0};

#include "vec_csv.h"

#define MAX_ROWS 400

struct table {
	double v[MAX_ROWS][MAXF];
	int n;
};

static struct table hr_ref, hrv_ref, rm_ref;

static void load(const char *csv, struct table *t)
{
	const char *p = next_line(csv);

	t->n = 0;
	while (p && *p && t->n < MAX_ROWS) {
		if (fields(p, t->v[t->n]) > 0) {
			t->n++;
		}
		p = next_line(p);
	}
}

/* ---- run the ports over one case ---- */

static double beats[MAX_ROWS];
static float beat_rr[MAX_ROWS];
static bool beat_clean[MAX_ROWS];
static int n_beats;

static void on_beat(void *ctx, double t, float rr, bool clean)
{
	ARG_UNUSED(ctx);
	if (n_beats < MAX_ROWS) {
		beats[n_beats] = t;
		beat_rr[n_beats] = rr;
		beat_clean[n_beats] = clean;
		n_beats++;
	}
}

static struct pulse_est pe;
static struct hrv hv;
static struct pulse_out outs[MAX_ROWS];
static int n_outs;
static struct hrv_window wins[MAX_ROWS];
static int n_wins;

static void run_case(const char *in, const char *hr, const char *hrvc, const char *rm)
{
	load(hr, &hr_ref);
	load(hrvc, &hrv_ref);
	load(rm, &rm_ref);
	pulse_init(&pe, 25);
	hv.cb = on_beat;
	hv.cb_ctx = NULL;
	hrv_init(&hv, 25);
	n_beats = n_outs = n_wins = 0;

	double last_t = 0, next_close = 15.0;
	const char *p = next_line(in);
	double f[MAXF];

	while (p && *p) {
		if (fields(p, f) >= 5) {
			int16_t acc[3] = {(int16_t)f[2], (int16_t)f[3], (int16_t)f[4]};

			last_t = f[0];
			if (pulse_push(&pe, (int32_t)f[1], acc) && n_outs < MAX_ROWS) {
				pulse_get(&pe, &outs[n_outs++]);
			}
			hrv_push(&hv, (int32_t)f[1]);
			/* close each 15 s window once its beats are final */
			if (hrv_time_s(&hv) >= next_close + 0.5) {
				wins[n_wins++] = hrv_close(&hv, next_close);
				next_close += 15.0;
			}
		}
		p = next_line(p);
	}
	hrv_flush(&hv);
	/* the reference keeps windows that end by the last sample */
	while (next_close <= last_t && n_wins < MAX_ROWS) {
		wins[n_wins++] = hrv_close(&hv, next_close);
		next_close += 15.0;
	}
}

static bool near_change(const struct table *t, int col, int i)
{
	for (int k = i - 2; k <= i + 2; k++) {
		if (k > 0 && k < t->n && t->v[k][col] != t->v[k - 1][col]) {
			return true;
		}
	}
	return false;
}

static void check_case(const char *name)
{
	/* hr.csv: t_s, bpm, conf, peak_bpm, acc_peak_bpm, raw_conf, raw_peak_bpm, pulse_present */
	zassert_equal(n_outs, hr_ref.n, "%s: %d outputs, reference %d", name, n_outs, hr_ref.n);
	int bpm_bad = 0, flag_bad = 0;

	for (int i = 0; i < hr_ref.n; i++) {
		double ref = hr_ref.v[i][1];
		double got = (double)outs[i].bpm;

		if (isnan(ref) != isnan(got) || (!isnan(ref) && fabs(ref - got) > 0.5)) {
			bpm_bad++;
			if (bpm_bad <= 3) {
				printk("%s t=%.0f bpm %.2f vs ref %.2f\n", name, hr_ref.v[i][0],
				       got, ref);
			}
		}
		if ((outs[i].pulse_present ? 1 : 0) != (int)hr_ref.v[i][7] && !near_change(&hr_ref, 7, i)) {
			flag_bad++;
		}
	}
	zassert_equal(bpm_bad, 0, "%s: %d bpm outside 0.5", name, bpm_bad);
	zassert_equal(flag_bad, 0, "%s: %d pulse_present flags differ", name, flag_bad);

	/* hrv.csv: beat_s, rr_ms, clean */
	zassert_equal(n_beats, hrv_ref.n, "%s: %d beats, reference %d", name, n_beats, hrv_ref.n);
	int t_bad = 0, c_bad = 0;

	for (int i = 0; i < hrv_ref.n; i++) {
		if (fabs(beats[i] - hrv_ref.v[i][0]) > 0.002) {
			t_bad++;
		}
		if (i > 0 && (beat_clean[i] ? 1 : 0) != (int)hrv_ref.v[i][2]) {
			c_bad++;
		}
	}
	zassert_equal(t_bad, 0, "%s: %d beat times outside 2 ms", name, t_bad);
	zassert_equal(c_bad, 0, "%s: %d clean flags differ", name, c_bad);

	/* rmssd.csv: start_s, rmssd_ms (empty below 5 differences), n_diffs */
	zassert_equal(n_wins, rm_ref.n, "%s: %d windows, reference %d", name, n_wins, rm_ref.n);
	for (int i = 0; i < rm_ref.n; i++) {
		double ref = rm_ref.v[i][1];

		zassert_equal(wins[i].n_diffs, (int)rm_ref.v[i][2], "%s window %d n_diffs", name, i);
		if (isnan(ref)) {
			zassert_equal(wins[i].rmssd_x10, 0, "%s window %d", name, i);
		} else {
			zassert_within(wins[i].rmssd_x10, (int)lround(ref * 10), 10,
				       "%s window %d: %u vs %.2f ms", name, i, wins[i].rmssd_x10, ref);
		}
	}
}

ZTEST_SUITE(ml_vectors, NULL, NULL, NULL, NULL, NULL);

ZTEST(ml_vectors, test_rest_64bpm)
{
	run_case(rest_in, rest_hr, rest_hrv, rest_rm);
	check_case("rest_64bpm");
}

ZTEST(ml_vectors, test_walk_72bpm)
{
	run_case(walk_in, walk_hr, walk_hrv, walk_rm);
	check_case("walk_72bpm_steps108");
}

ZTEST(ml_vectors, test_run_140bpm)
{
	run_case(run_in, run_hr, run_hrv, run_rm);
	check_case("run_140bpm_steps168");
}

ZTEST(ml_vectors, test_hrv_rest_58bpm)
{
	run_case(hrv_in, hrv_hr, hrv_hrv, hrv_rm);
	check_case("hrv_rest_58bpm_rmssd45");
}

ZTEST(ml_vectors, test_offbody_noise)
{
	run_case(off_in, off_hr, off_hrv, off_rm);
	check_case("offbody_noise");
}
