/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sigq.h"

#include <math.h>
#include <string.h>

void sigq_init(struct sigq *s, const struct sigq_cfg *green, const struct sigq_cfg *ir)
{
	s->cfg[0] = *green;
	s->cfg[1] = *ir;
	sigq_reset(s);
}

void sigq_reset(struct sigq *s)
{
	memset(s->bp, 0, sizeof(s->bp));
	memset(s->raw, 0, sizeof(s->raw));
	s->head = 0;
	s->n = 0;
}

void sigq_push(struct sigq *s, int32_t raw, float bp)
{
	s->bp[s->head] = bp;
	s->raw[s->head] = raw;
	s->head = (uint16_t)((s->head + 1) % SIGQ_WIN);
	if (s->n < SIGQ_WIN) {
		s->n++;
	}
}

static float ramp(float v, float lo, float hi)
{
	float f = (v - lo) / (hi - lo);

	return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

void sigq_eval(const struct sigq *s, const struct sigq_in *in, struct sigq_out *out)
{
	const struct sigq_cfg *c = &s->cfg[in->ir ? 1 : 0];
	float x[SIGQ_WIN];
	double sum = 0.0, bsum = 0.0, bsq = 0.0;
	int32_t max = INT32_MIN;

	out->q = SIGQ_NONE;
	out->flags = (in->ir ? SIGQ_IR : 0) | (in->motion_mg > c->max_motion_mg ? SIGQ_MOVING : 0) |
		     (in->pulse_present ? SIGQ_PULSE : 0);
	out->pi = NAN;
	out->rho = NAN;
	if (s->n < SIGQ_WIN) {
		return;
	}
	/* oldest first */
	for (int i = 0; i < SIGQ_WIN; i++) {
		int k = (s->head + i) % SIGQ_WIN;

		x[i] = s->bp[k];
		sum += s->raw[k];
		bsum += (double)x[i];
		bsq += (double)x[i] * (double)x[i];
		max = s->raw[k] > max ? s->raw[k] : max;
	}
	double dc = sum / SIGQ_WIN;
	float mean = (float)(bsum / SIGQ_WIN);
	float e = 0.0f;

	for (int i = 0; i < SIGQ_WIN; i++) {
		x[i] -= mean;
		e += x[i] * x[i];
	}
	/* regularity: the best normalised autocorrelation at 40-180 bpm */
	float rho = 0.0f;

	if (e > 0.0f) {
		rho = -INFINITY;
		for (int k = SIGQ_LAG_LO; k <= SIGQ_LAG_HI; k++) {
			float acc = 0.0f;

			for (int i = 0; i + k < SIGQ_WIN; i++) {
				acc += x[i] * x[i + k];
			}
			float r = acc / e * (float)SIGQ_WIN / (float)(SIGQ_WIN - k);

			rho = r > rho ? r : rho;
		}
	}
	float ac = sqrtf((float)(bsq / SIGQ_WIN) * (rho > 0.0f ? rho : 0.0f));
	float pi = dc > 0.0 ? ac / (float)dc : 0.0f;

	out->pi = pi;
	out->rho = rho;
	if ((float)max > (1.0f - c->adc_margin) * c->adc_full) {
		out->flags |= SIGQ_SATURATED;
	}
	if (in->led_at_max && dc < (double)(c->dark_frac * c->adc_full)) {
		out->flags |= SIGQ_DARK;
	}
	if (out->flags & (SIGQ_SATURATED | SIGQ_DARK)) {
		out->q = 0;
		return;
	}
	float s_pi = ramp(log10f(pi > 1e-9f ? pi : 1e-9f), log10f(c->pi_lo), log10f(c->pi_hi));
	float s_rho = ramp(rho, c->rho_lo, c->rho_hi);

	out->q = (uint8_t)lroundf(100.0f * s_pi * s_rho);
	if (out->q >= c->good_q && (out->flags & (SIGQ_PULSE | SIGQ_MOVING)) == SIGQ_PULSE) {
		out->flags |= SIGQ_GOOD;
	}
}
