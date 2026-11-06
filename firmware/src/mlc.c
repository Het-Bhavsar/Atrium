/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
 *
 * mlc_program below is ST's motion_intensity example for the LSM6DSOX
 * (st-mems-machine-learning-core, examples/motion_intensity/lsm6dsox), with Atrium's
 * seven thresholds in the tree and the interrupt routed to INT2 (atrium_ml/mlc.py
 * atrium_config()). ST's notice for that program:
 *
 * Copyright (c) 2025, STMicroelectronics. All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met: 1. Redistributions of source
 * code must retain the above copyright notice, this list of conditions and the following
 * disclaimer. 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution. 3. Neither the name of the copyright holder nor
 * the names of its contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission. THIS SOFTWARE IS PROVIDED BY THE
 * COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
#include "mlc.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979f

const struct mlc_write mlc_program[] = {
	{0x10, 0x00}, {0x11, 0x00}, {0x01, 0x80}, {0x04, 0x00}, {0x05, 0x00}, {0x17, 0x40},
	{0x02, 0x11}, {0x08, 0xEA}, {0x09, 0x46}, {0x09, 0x03}, {0x09, 0x50}, {0x09, 0x03},
	{0x09, 0x00}, {0x09, 0x00}, {0x09, 0x0A}, {0x02, 0x11}, {0x08, 0xF2}, {0x09, 0x27},
	{0x02, 0x11}, {0x08, 0xFA}, {0x09, 0x3C}, {0x09, 0x03}, {0x09, 0x52}, {0x09, 0x03},
	{0x09, 0x5E}, {0x09, 0x03}, {0x02, 0x31}, {0x08, 0x3C}, {0x09, 0x3F}, {0x09, 0x00},
	{0x09, 0x01}, {0x09, 0x10}, {0x09, 0x00}, {0x09, 0x00}, {0x09, 0x00}, {0x09, 0x00},
	{0x09, 0x1F}, {0x09, 0x00}, {0x02, 0x31}, {0x08, 0x52}, {0x09, 0x00}, {0x09, 0x00},
	{0x09, 0x00}, {0x09, 0x00}, {0x09, 0x00}, {0x09, 0x00}, {0x09, 0x00}, {0x09, 0x00},
	{0x09, 0x00}, {0x09, 0x00}, {0x09, 0x00}, {0x01, 0x00}, {0x01, 0x80}, {0x17, 0x40},
	{0x02, 0x31}, {0x08, 0x5E}, {0x09, 0x8E}, {0x09, 0x06}, {0x09, 0x10}, {0x09, 0xC0},
	{0x09, 0x19}, {0x09, 0x14}, {0x09, 0x21}, {0x09, 0xC0}, {0x09, 0x19}, {0x09, 0x1C},
	{0x09, 0x32}, {0x09, 0xC0}, {0x09, 0x1F}, {0x09, 0x21}, {0x09, 0x43}, {0x09, 0xC0},
	{0x09, 0xAE}, {0x09, 0x27}, {0x09, 0x54}, {0x09, 0xC0}, {0x09, 0x1F}, {0x09, 0x2D},
	{0x09, 0x65}, {0x09, 0xC0}, {0x09, 0x00}, {0x09, 0x34}, {0x09, 0x76}, {0x09, 0xE0},
	{0x01, 0x80}, {0x17, 0x00}, {0x04, 0x00}, {0x05, 0x10}, {0x02, 0x01}, {0x01, 0x00},
	{0x5F, 0x02}, {0x01, 0x80}, {0x11, 0x01}, {0x60, 0x05}, {0x01, 0x00}, {0x10, 0x10},
	{0x11, 0x00},
};
const size_t mlc_program_len = sizeof(mlc_program) / sizeof(mlc_program[0]);

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

/* the 28 page writes after PAGE_SEL 0x31, PAGE_ADDRESS 0x5E hold the 7 nodes, threshold first */
static int tree_start(void)
{
	for (size_t i = 0; i + 1 < mlc_program_len; i++) {
		if (mlc_program[i].reg == 0x02 && mlc_program[i].val == 0x31 &&
		    mlc_program[i + 1].reg == 0x08 && mlc_program[i + 1].val == 0x5E) {
			return (int)i + 2;
		}
	}
	return -1;
}

static float half_to_float(uint16_t h)
{
	int e = (h >> 10) & 0x1F;
	float m = (float)(h & 0x3FF);
	float v = e == 0 ? ldexpf(m, -24) : ldexpf(m + 1024.0f, e - 25);

	return (h & 0x8000) ? -v : v;
}

float mlc_threshold(int k)
{
	int s = tree_start();

	if (s < 0 || k < 0 || k > 6) {
		return NAN;
	}
	return half_to_float((uint16_t)(mlc_program[s + 4 * k].val |
					(mlc_program[s + 4 * k + 1].val << 8)));
}

uint8_t mlc_level_of(float variance_g2)
{
	uint8_t n = 0;

	for (int k = 0; k < 7; k++) {
		n += mlc_threshold(k) < variance_g2;
	}
	return n;
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
