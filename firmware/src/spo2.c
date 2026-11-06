/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "spo2.h"

#include <math.h>
#include <string.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* scipy.signal.butter(2, [0.5, 4.0], "bandpass", fs=25, output="sos") */
static const double sos[2][6] = {
	{0.11735103672460913, 0.23470207344921826, 0.11735103672460913, 1.0, -0.8071314388538938,
	 0.34797343307831957},
	{1.0, -2.0, 1.0, 1.0, -1.8291984415883695, 0.846721328641201},
};

/* transposed direct form II, as sosfilt. Fed x - x0 from zero state, which equals
 * sosfilt primed with sosfilt_zi * x0 (the band-pass has no DC gain) */
static double bandpass(double z[2][2], double x)
{
	for (int k = 0; k < 2; k++) {
		const double *c = sos[k];
		double y = c[0] * x + z[k][0];

		z[k][0] = c[1] * x - c[4] * y + z[k][1];
		z[k][1] = c[2] * x - c[5] * y;
		x = y;
	}
	return x;
}

static void block_reset(struct spo2_block *b)
{
	memset(b, 0, sizeof(*b));
	b->rmin = b->imin = INT32_MAX;
	b->rmax = b->imax = INT32_MIN;
	b->pulse_all = true;
}

void spo2_init(struct spo2 *s, const struct spo2_cfg *cfg)
{
	memset(s, 0, sizeof(*s));
	s->cfg = *cfg;
	block_reset(&s->prev);
	block_reset(&s->cur);
}

void spo2_gate(struct spo2 *s, float motion_mg, bool pulse_ok)
{
	s->gate_motion = motion_mg;
	s->gate_pulse = pulse_ok;
	s->have_gate = true;
	if (s->sec_open) {
		s->sec_motion = fmaxf(s->sec_motion, motion_mg);
		s->sec_pulse = s->sec_pulse && pulse_ok;
	}
}

static void eval_window(struct spo2 *s)
{
	const struct spo2_cfg *c = &s->cfg;
	const struct spo2_block *a = &s->prev, *b = &s->cur;
	const double n = 2.0 * SPO2_BLOCK;
	double mr = (a->sr + b->sr) / n, mi = (a->si + b->si) / n;
	double qr = (a->srr + b->srr) / n, qi = (a->sii + b->sii) / n;
	double ar = sqrt(qr), ai = sqrt(qi);
	double r_dc = (a->dr + b->dr) / n, i_dc = (a->di + b->di) / n;
	double corr = 0.0;

	if (ar > 0.0 && ai > 0.0) {
		double cov = (a->sri + b->sri) / n - mr * mi;
		double vr = qr - mr * mr, vi = qi - mi * mi;

		corr = vr > 0.0 && vi > 0.0 ? cov / sqrt(vr * vi) : 0.0;
	}
	double pi_ir = i_dc > 0.0 ? ai / i_dc : 0.0;
	double r = r_dc > 0.0 && pi_ir > 0.0 ? (ar / r_dc) / pi_ir : (double)NAN;
	double lo = (double)c->adc_margin * (double)c->adc_full;
	double hi = (1.0 - (double)c->adc_margin) * (double)c->adc_full;
	bool in_range = lo < MIN(a->rmin, b->rmin) && MAX(a->rmax, b->rmax) < hi &&
			lo < MIN(a->imin, b->imin) && MAX(a->imax, b->imax) < hi;
	bool still = fmaxf(a->motion_max, b->motion_max) <= c->max_motion_mg;
	bool pulse = a->pulse_all && b->pulse_all;
	struct spo2_window *w = &s->win[s->n_win++];

	w->t_s = (float)s->n / SPO2_FS;
	w->r = (float)r;
	w->corr = (float)corr;
	w->pi_ir = (float)pi_ir;
	w->good = isfinite(r) && corr >= (double)c->min_corr && pi_ir >= (double)c->min_pi &&
		  still && pulse && in_range;
}

void spo2_push(struct spo2 *s, int32_t red, int32_t ir)
{
	struct spo2_block *b = &s->cur;

	if (spo2_done(s)) {
		return;
	}
	if (s->n == 0) {
		s->r0 = red;
		s->i0 = ir;
	}
	if (s->n % SPO2_FS == 0) {
		/* a new second starts with the latest gate; no gate at all means no veto */
		s->sec_open = true;
		s->sec_motion = s->have_gate ? s->gate_motion : 0.0f;
		s->sec_pulse = s->have_gate ? s->gate_pulse : true;
	}
	double fr = bandpass(s->zr, (double)(red - s->r0));
	double fi = bandpass(s->zi, (double)(ir - s->i0));

	b->sr += fr;
	b->si += fi;
	b->srr += fr * fr;
	b->sii += fi * fi;
	b->sri += fr * fi;
	b->dr += red;
	b->di += ir;
	b->rmin = MIN(b->rmin, red);
	b->rmax = MAX(b->rmax, red);
	b->imin = MIN(b->imin, ir);
	b->imax = MAX(b->imax, ir);
	s->n++;

	if (s->n % SPO2_FS == 0) {
		b->motion_max = fmaxf(b->motion_max, s->sec_motion);
		b->pulse_all = b->pulse_all && s->sec_pulse;
		s->sec_open = false;
	}
	if (s->n % SPO2_BLOCK == 0) {
		if (s->n >= SPO2_SETTLE + 2 * SPO2_BLOCK) {
			eval_window(s);
		}
		s->prev = *b;
		block_reset(b);
	}
}

bool spo2_done(const struct spo2 *s)
{
	return s->n >= SPO2_SAMPLES;
}

static void sort_f(float *v, int n)
{
	for (int i = 1; i < n; i++) {
		float x = v[i];
		int j = i - 1;

		for (; j >= 0 && v[j] > x; j--) {
			v[j + 1] = v[j];
		}
		v[j + 1] = x;
	}
}

/* np.percentile with linear interpolation, on sorted v */
static float pct(const float *v, int n, float q)
{
	float pos = q * (float)(n - 1);
	int i = (int)pos;
	float f = pos - (float)i;

	return i + 1 < n ? v[i] + f * (v[i + 1] - v[i]) : v[n - 1];
}

void spo2_result(const struct spo2 *s, struct spo2_out *out)
{
	float good[SPO2_WINDOWS];
	int ng = 0;

	for (int k = 0; k < s->n_win; k++) {
		if (s->win[k].good) {
			good[ng++] = s->win[k].r;
		}
	}
	out->n_good = (uint8_t)ng;
	out->n_windows = s->n_win;
	out->r = NAN;
	out->confidence = 0.0f;
	if (ng < s->cfg.min_good || ng == 0) {
		return;
	}
	sort_f(good, ng);
	float med = pct(good, ng, 0.5f);
	float spread = med > 0.0f ? (pct(good, ng, 0.75f) - pct(good, ng, 0.25f)) / med : INFINITY;
	float q = 1.0f - spread / s->cfg.max_spread;

	q = q < 0.0f ? 0.0f : q > 1.0f ? 1.0f : q;
	out->r = med;
	out->confidence = ((float)ng / (float)s->n_win) * q;
}

void spo2_sched_init(struct spo2_sched *q, uint32_t now_s)
{
	memset(q, 0, sizeof(*q));
	q->mode = MODE_COUNT;
	q->moved_s = now_s;
	q->next_s = now_s;
}

void spo2_sched_level(struct spo2_sched *q, uint32_t now_s, uint8_t level)
{
	q->level_s = now_s;
	q->have_level = true;
	if (level) {
		q->moved_s = now_s;
	}
}

bool spo2_sched_tick(struct spo2_sched *q, uint32_t now_s, enum pod_mode mode, bool can,
		     bool pulse_present)
{
	uint32_t period = mode == MODE_SLEEP ? SPO2_PERIOD_SLEEP_S
			  : mode == MODE_SMART ? SPO2_PERIOD_SMART_S : 0;

	if (mode != q->mode) {
		/* a new mode waits one period before its first burst */
		q->mode = mode;
		q->next_s = now_s + period;
	}
	if (period == 0 || (int32_t)(now_s - q->next_s) < 0) {
		return false;
	}
	/* the MLC updates every 3.12 s: a level older than 10 s means the IMU is not running */
	bool still = q->have_level && now_s - q->level_s <= 10 && now_s - q->moved_s >= SPO2_STILL_S;

	if (can && still && pulse_present) {
		q->next_s = now_s + period;
		return true;
	}
	q->next_s = now_s + SPO2_RETRY_S;
	return false;
}
