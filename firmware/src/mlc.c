/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mlc.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979f

/* The seven thresholds of Atrium's tree, as the half floats the LSM6DSOX program carried
 * (atrium_ml/mlc.py atrium_config()): 1e-4, 1e-3, 4e-3, 1e-2, 3e-2, 8e-2 and 0.25 g^2 at half
 * precision. Kept bit for bit, so the levels match the ones the IMU gave. */
static const uint16_t thresholds_half[7] = {0x068E, 0x1419, 0x1C19, 0x211F, 0x27AE, 0x2D1F, 0x3400};

/* ClassConfig defaults [ASSUMED, bench V2] */
#define PERIODIC_SHARE 0.65f
#define WALK_LV_MIN    4
#define WALK_LV_MAX    5
#define RUN_LV_MIN     6
#define WALK_SPM_MIN   70.0f
#define WALK_SPM_MAX   150.0f
#define RUN_SPM_MIN    140.0f
#define RUN_SPM_MAX    200.0f

#define NFFT    128
#define BAND_LO 1.0f
#define BAND_HI 3.5f
#define NEAR_HZ 0.15f

static float half_to_float(uint16_t h)
{
	int e = (h >> 10) & 0x1F;
	float m = (float)(h & 0x3FF);
	float v = e == 0 ? ldexpf(m, -24) : ldexpf(m + 1024.0f, e - 25);

	return (h & 0x8000) ? -v : v;
}

float mlc_threshold(int k)
{
	if (k < 0 || k > 6) {
		return NAN;
	}
	return half_to_float(thresholds_half[k]);
}

uint8_t mlc_level_of(float variance_g2)
{
	uint8_t n = 0;

	for (int k = 0; k < 7; k++) {
		n += mlc_threshold(k) < variance_g2;
	}
	return n;
}

static uint8_t len_at(uint32_t fs_mhz, uint32_t ms, uint32_t max)
{
	uint32_t n = (uint32_t)(((uint64_t)fs_mhz * ms + 500000U) / 1000000U);

	return (uint8_t)(n < 1 ? 1 : n > max ? max : n);
}

uint8_t mlc_window_len(uint32_t fs_mhz)
{
	return len_at(fs_mhz, MLC_WINDOW_MS, UINT8_MAX);
}

uint8_t mlc_rhythm_len(uint32_t fs_mhz)
{
	return len_at(fs_mhz, MLC_RHYTHM_MS, MLC_RHYTHM_MAX);
}

void mlc_win_init(struct mlc_win *w, uint8_t len)
{
	memset(w, 0, sizeof(*w));
	w->len = len ? len : 1;
}

bool mlc_win_push(struct mlc_win *w, float norm_g, uint8_t *level)
{
	if (w->n == 0) {
		w->x0 = norm_g;
	}
	float d = norm_g - w->x0;

	w->s += d;
	w->s2 += d * d;
	if (++w->n < w->len) {
		return false;
	}
	float m = w->s / w->n;
	float var = w->s2 / w->n - m * m; /* population variance, as np.var */

	*level = mlc_level_of(var > 0.0f ? var : 0.0f);
	mlc_win_init(w, w->len);
	return true;
}

void mlc_record_reset(struct mlc_record *r)
{
	memset(r, 0, sizeof(*r));
}

void mlc_record_add(struct mlc_record *r, uint8_t level)
{
	if (level < MLC_LEVELS && r->n < UINT8_MAX) {
		r->hist[level]++;
		r->n++;
	}
}

uint8_t mlc_record_max(const struct mlc_record *r)
{
	for (int k = MLC_LEVELS - 1; k > 0; k--) {
		if (r->hist[k]) {
			return (uint8_t)k;
		}
	}
	return 0;
}

/* value at sorted position i (0-based) */
static uint8_t nth(const struct mlc_record *r, unsigned int i)
{
	unsigned int seen = 0;

	for (int k = 0; k < MLC_LEVELS; k++) {
		seen += r->hist[k];
		if (i < seen) {
			return (uint8_t)k;
		}
	}
	return MLC_LEVELS - 1;
}

uint8_t mlc_record_median(const struct mlc_record *r)
{
	if (r->n == 0) {
		return 0;
	}
	if (r->n % 2) {
		return nth(r, r->n / 2);
	}
	return (uint8_t)((nth(r, r->n / 2 - 1) + nth(r, r->n / 2)) / 2);
}

bool mlc_step_rhythm(const float *norm_g, size_t n, float fs, float *spm, float *share)
{
	float df = fs / NFFT;
	int k_lo = (int)ceilf(BAND_LO / df);
	int k_hi = (int)floorf(BAND_HI / df);
	/* bins k_lo - 1 .. k_hi + 1, for the parabola at the band edges */
	float p[NFFT / 2 + 1];
	float mean = 0.0f;

	if (n == 0 || n > NFFT || k_lo < 1 || k_hi + 1 > NFFT / 2) {
		return false;
	}
	for (size_t i = 0; i < n; i++) {
		mean += norm_g[i];
	}
	mean /= (float)n;
	for (int k = k_lo - 1; k <= k_hi + 1; k++) {
		float re = 0.0f, im = 0.0f;

		for (size_t i = 0; i < n; i++) {
			/* np.hanning: symmetric, N - 1 in the denominator */
			float w = n > 1 ? 0.5f - 0.5f * cosf(2.0f * PI_F * (float)i / (float)(n - 1)) : 1.0f;
			float x = (norm_g[i] - mean) * w;
			float a = 2.0f * PI_F * (float)((k * (int)i) % NFFT) / NFFT;

			re += x * cosf(a);
			im -= x * sinf(a);
		}
		p[k] = re * re + im * im;
	}

	int pk = k_lo;
	float total = 0.0f;

	for (int k = k_lo; k <= k_hi; k++) {
		total += p[k];
		if (p[k] > p[pk]) {
			pk = k;
		}
	}
	if (!(total > 0.0f)) {
		return false;
	}
	int h = (int)lroundf(NEAR_HZ / df);
	float near = 0.0f;

	for (int k = k_lo; k <= k_hi; k++) {
		if (k >= pk - h && k <= pk + h) {
			near += p[k];
		}
	}
	float a = p[pk - 1], b = p[pk], c = p[pk + 1];
	float den = a - 2.0f * b + c;
	float frac = den != 0.0f ? 0.5f * (a - c) / den : 0.0f;

	frac = frac > 0.5f ? 0.5f : frac < -0.5f ? -0.5f : frac;
	*spm = ((float)pk + frac) * df * 60.0f;
	*share = near / total;
	return true;
}

enum mlc_class mlc_activity_class(const struct mlc_record *r, bool rhythm_ok, float spm,
				  float share)
{
	if (r->n == 0 || mlc_record_max(r) == 0) {
		return MLC_STILL;
	}
	bool periodic = rhythm_ok && share >= PERIODIC_SHARE;
	uint8_t top = mlc_record_median(r);

	if (periodic && top >= RUN_LV_MIN && spm >= RUN_SPM_MIN && spm <= RUN_SPM_MAX) {
		return MLC_RUN;
	}
	if (periodic && top >= WALK_LV_MIN && top <= WALK_LV_MAX && spm >= WALK_SPM_MIN &&
	    spm <= WALK_SPM_MAX) {
		return MLC_WALK;
	}
	return MLC_OTHER;
}
