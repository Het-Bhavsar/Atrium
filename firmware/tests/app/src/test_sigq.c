/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * sigq.c against app/python/atrium_ml/fit.py on the fit_* vectors from make_test_vectors.py:
 * every second from 4 s, q within 2 and the flags equal (the ML hand-over tolerance), pi and rho
 * close. The samples go through pulse.c's band-pass, as on the pod. Plus the GATT bytes
 * (docs/ble_gatt.md 4.2) and the pulse helpers sigq uses.
 */
#include <math.h>
#include <zephyr/ztest.h>

#include "gatt_encode.h"
#include "pulse.h"
#include "sigq.h"
#include "vec_csv.h"

static const char good_in[] = {
#include "vectors/fit_good_input.inc"
	0};
static const char good_out[] = {
#include "vectors/fit_good.inc"
	0};
static const char weak_in[] = {
#include "vectors/fit_weak_input.inc"
	0};
static const char weak_out[] = {
#include "vectors/fit_weak.inc"
	0};
static const char noisy_in[] = {
#include "vectors/fit_noisy_input.inc"
	0};
static const char noisy_out[] = {
#include "vectors/fit_noisy.inc"
	0};
static const char moving_in[] = {
#include "vectors/fit_moving_input.inc"
	0};
static const char moving_out[] = {
#include "vectors/fit_moving.inc"
	0};
static const char sat_in[] = {
#include "vectors/fit_saturated_input.inc"
	0};
static const char sat_out[] = {
#include "vectors/fit_saturated.inc"
	0};
static const char dark_in[] = {
#include "vectors/fit_dark_input.inc"
	0};
static const char dark_out[] = {
#include "vectors/fit_dark.inc"
	0};

static struct pulse_est p;
static struct sigq sq;
static const struct sigq_cfg cfg = SIGQ_CFG_GREEN;

ZTEST_SUITE(sigq, NULL, NULL, NULL, NULL, NULL);

static void run_case(const char *name, const char *in, const char *exp)
{
	double f[MAXF], e[MAXF];
	const char *ep = next_line(exp);
	int k = 0, secs = 0;

	pulse_init(&p, PULSE_FS);
	sigq_init(&sq, &cfg, &cfg);
	for (const char *ip = next_line(in); ip && *ip; ip = next_line(ip)) {
		zassert_true(fields(ip, f) >= 4);
		(void)pulse_push(&p, (int32_t)f[1], NULL);
		sigq_push(&sq, (int32_t)f[1], pulse_last_bp(&p));
		k++;
		if (k < SIGQ_WIN || k % SIGQ_FS) {
			continue;
		}
		struct pulse_out po;
		struct sigq_in si = {.motion_mg = (float)f[2], .led_at_max = f[3] != 0.0};
		struct sigq_out o;

		si.pulse_present = pulse_get(&p, &po) && po.pulse_present;
		sigq_eval(&sq, &si, &o);
		zassert_not_null(ep, "%s: more seconds than expected", name);
		int ne = fields(ep, e);

		/* t_s, q, pi, rho, moving, saturated, dark, pulse_present, good */
		zassert_equal(ne, 9, "%s columns", name);
		zassert_equal(!!(o.flags & SIGQ_PULSE), e[7] != 0.0, "%s %.0f s pulse", name, e[0]);
		zassert_equal(!!(o.flags & SIGQ_GOOD), e[8] != 0.0, "%s %.0f s good", name, e[0]);
		zassert_within((double)k / SIGQ_FS, e[0], 1e-6, "%s time", name);
		zassert_within((int)o.q, (int)e[1], 2, "%s %.0f s: q %d, fit.py %.0f", name, e[0], o.q,
			       e[1]);
		zassert_within((double)o.pi, e[2], e[2] * 0.02 + 2e-6, "%s %.0f s: pi %f / %f", name,
			       e[0], (double)o.pi, e[2]);
		zassert_within((double)o.rho, e[3], 0.02, "%s %.0f s: rho %f / %f", name, e[0],
			       (double)o.rho, e[3]);
		zassert_equal(!!(o.flags & SIGQ_MOVING), e[4] != 0.0, "%s %.0f s moving", name, e[0]);
		zassert_equal(!!(o.flags & SIGQ_SATURATED), e[5] != 0.0, "%s %.0f s saturated", name,
			      e[0]);
		zassert_equal(!!(o.flags & SIGQ_DARK), e[6] != 0.0, "%s %.0f s dark", name, e[0]);
		ep = next_line(ep);
		secs++;
	}
	zassert_equal(k, 30 * SIGQ_FS, "%s: 30 s at 25 Hz", name);
	zassert_equal(secs, 27, "%s: one value a second from 4 s", name);
}

ZTEST(sigq, test_fit_vectors)
{
	run_case("good", good_in, good_out);
	run_case("weak", weak_in, weak_out);
	run_case("noisy", noisy_in, noisy_out);
	run_case("moving", moving_in, moving_out);
	run_case("saturated", sat_in, sat_out);
	run_case("dark", dark_in, dark_out);
}

ZTEST(sigq, test_not_measuring)
{
	struct sigq_in in = {.motion_mg = 40.0f, .ir = true};
	struct sigq_out o;

	sigq_init(&sq, &cfg, &cfg);
	for (int i = 0; i < SIGQ_WIN - 1; i++) {
		sigq_push(&sq, 200000, 0.0f);
	}
	sigq_eval(&sq, &in, &o);
	zassert_equal(o.q, SIGQ_NONE, "under 4 s");
	zassert_true(isnan(o.pi) && isnan(o.rho));
	zassert_equal(o.flags, SIGQ_MOVING | SIGQ_IR, "the flags are live anyway");

	sigq_push(&sq, 200000, 0.0f);
	in.motion_mg = 0.0f;
	sigq_eval(&sq, &in, &o);
	zassert_equal(o.q, 0, "a flat window: no pulse");
	zassert_equal(o.rho, 0.0f);

	sigq_reset(&sq);
	sigq_eval(&sq, &in, &o);
	zassert_equal(o.q, SIGQ_NONE, "a mode change starts a new window");
}

ZTEST(sigq, test_fit_good)
{
	struct sigq_in in = {.pulse_present = true, .motion_mg = 5.0f};
	struct sigq_out o;
	struct sigq_cfg ir = cfg;

	/* a 1.2 Hz pulse at 0.2 % perfusion: regular and strong */
	ir.good_q = 101; /* IR's own constants: never good */
	sigq_init(&sq, &cfg, &ir);
	for (int i = 0; i < SIGQ_WIN; i++) {
		float bp = 400.0f * sinf(2.0f * 3.14159265f * 1.2f * i / SIGQ_FS);

		sigq_push(&sq, 200000 - (int32_t)bp, bp);
	}
	sigq_eval(&sq, &in, &o);
	zassert_true(o.q >= 60, "q %d", o.q);
	zassert_equal(o.flags, SIGQ_PULSE | SIGQ_GOOD);

	in.pulse_present = false; /* the slow test has not passed yet */
	sigq_eval(&sq, &in, &o);
	zassert_equal(o.flags, 0);

	in.pulse_present = true;
	in.motion_mg = 40.0f;
	sigq_eval(&sq, &in, &o);
	zassert_equal(o.flags, SIGQ_PULSE | SIGQ_MOVING, "hold still");

	in.motion_mg = 5.0f;
	in.ir = true;
	sigq_eval(&sq, &in, &o);
	zassert_equal(o.flags, SIGQ_PULSE | SIGQ_IR, "the IR constants apply");

	in.ir = false;
	in.led_at_max = true;
	sigq_reset(&sq);
	for (int i = 0; i < SIGQ_WIN; i++) {
		float bp = 4.0f * sinf(2.0f * 3.14159265f * 1.2f * i / SIGQ_FS);

		sigq_push(&sq, 2000 - (int32_t)bp, bp); /* under 1 % of full scale */
	}
	sigq_eval(&sq, &in, &o);
	zassert_equal(o.q, 0);
	zassert_equal(o.flags, SIGQ_PULSE | SIGQ_DARK, "dark is never good");
}

ZTEST(sigq, test_gatt_bytes)
{
	struct app_status s = {.sq = {.q = 74, .flags = SIGQ_MOVING | SIGQ_IR, .pi = 0.001639f,
				      .rho = 0.8845f}};
	uint8_t b[GATT_FIT_QUALITY_LEN];

	zassert_equal(gatt_encode_fit_quality(&s, b), 4);
	zassert_equal(b[0], 74);
	zassert_equal(b[1], 0x09);
	zassert_equal(b[2], 16, "0.16 %% in 0.01 %% steps");
	zassert_equal(b[3], 88, "rho x 100");

	s.sq = (struct sigq_out){.q = SIGQ_NONE, .pi = NAN, .rho = NAN};
	gatt_encode_fit_quality(&s, b);
	zassert_equal(b[0], 255, "not measuring");
	zassert_equal(b[2], 0);
	zassert_equal(b[3], 0);

	s.sq = (struct sigq_out){.q = 0, .flags = SIGQ_DARK, .pi = 0.04f, .rho = 3.0f};
	gatt_encode_fit_quality(&s, b);
	zassert_equal(b[2], 255, "pi clamps at 2.55 %%");
	zassert_equal(b[3], 255);
}

ZTEST(sigq, test_pulse_helpers)
{
	static const int16_t still[3] = {0, 0, 1000};
	int16_t shake[3];

	pulse_init(&p, 8); /* the off-body probe: no estimate, no bar */
	(void)pulse_push(&p, 1000, still);
	zassert_equal(pulse_last_bp(&p), 0.0f);
	zassert_equal(pulse_recent_motion_mg(&p, PULSE_FS), 0.0f);

	pulse_init(&p, PULSE_FS);
	for (int i = 0; i < 4 * PULSE_FS; i++) {
		(void)pulse_push(&p, 100000 + (i % 5) * 100, still);
	}
	zassert_equal(pulse_last_bp(&p), p.x[(p.head + PULSE_WIN - 1) % PULSE_WIN]);
	zassert_true(pulse_last_bp(&p) != 0.0f);
	zassert_true(pulse_recent_motion_mg(&p, PULSE_FS) < 1.0f, "still");

	/* one second of 2 Hz shaking at 100 mg on x */
	for (int i = 0; i < PULSE_FS; i++) {
		shake[0] = (int16_t)lroundf(100.0f * sinf(2.0f * 3.14159265f * 2.0f * i / PULSE_FS));
		shake[1] = 0;
		shake[2] = 1000;
		(void)pulse_push(&p, 100000, shake);
	}
	float m1 = pulse_recent_motion_mg(&p, PULSE_FS);

	zassert_true(m1 > 30.0f, "moving: %f mg", (double)m1);
	/* a second still again: the 1 s motion drops, so "hold still" clears */
	for (int i = 0; i < PULSE_FS; i++) {
		(void)pulse_push(&p, 100000, still);
	}
	zassert_true(pulse_recent_motion_mg(&p, PULSE_FS) < m1 / 2.0f);
	zassert_equal(pulse_recent_motion_mg(&p, PULSE_WIN + 1), 0.0f);
}

/* Heart Rate 0x2A37 in the fit guide: the pulse before wear says worn, never noise off it */
ZTEST(sigq, test_report_bpm)
{
	struct pulse_out po = {.bpm = 71.6f, .valid = true, .pulse_present = true};

	zassert_equal(pulse_report_bpm(&po, true, false), 72, "worn");
	zassert_equal(pulse_report_bpm(&po, false, false), 0, "off the body: not reported or logged");
	zassert_equal(pulse_report_bpm(&po, false, true), 72, "fit guide, pulse present");
	po.pulse_present = false;
	zassert_equal(pulse_report_bpm(&po, false, true), 0, "fit guide, a noise peak");
	zassert_equal(pulse_report_bpm(&po, true, false), 72, "worn: pulse presence gates wear only");
	po.valid = false;
	zassert_equal(pulse_report_bpm(&po, true, true), 0, "low confidence");
	po = (struct pulse_out){.bpm = NAN, .valid = true, .pulse_present = true};
	zassert_equal(pulse_report_bpm(&po, true, true), 0, "no track yet");
	po.bpm = 300.0f;
	zassert_equal(pulse_report_bpm(&po, true, false), 255);
}
