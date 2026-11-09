/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "hrv.h"

#include <math.h>
#include <string.h>

/* HrvConfig in atrium_ml/hrv.py */
#define REFRACTORY 8   /* round(0.33 s x 25 Hz) samples */
#define SETTLE     50  /* 2 s */
#define THR_FRAC   0.4f
#define RR_MIN_MS  300.0f
#define RR_MAX_MS  2000.0f
#define MAX_DEV    0.15f

/* Butterworth band-pass 0.5-5 Hz at 25 Hz, order 2, scipy.signal.butter / sosfilt_zi */
static const float sos[2][6] = {
	{0.1750876436721008f, 0.3501752873442016f, 0.1750876436721008f, 1.0f,
	 -0.47338545635197227f, 0.26070108314598184f},
	{1.0f, -2.0f, 1.0f, 1.0f, -1.8256698996865246f, 0.8425510982273042f},
};
static const float zi[2][2] = {
	{0.7144546832600692f, -0.05681700446331302f},
	{-0.8895423269321678f, 0.8895423269321682f},
};

static float median(const float *v, int n)
{
	float s[8];

	memcpy(s, v, n * sizeof(float));
	for (int i = 1; i < n; i++) {
		float x = s[i];
		int j = i - 1;

		for (; j >= 0 && s[j] > x; j--) {
			s[j + 1] = s[j];
		}
		s[j + 1] = x;
	}
	return (n & 1) ? s[n / 2] : 0.5f * (s[n / 2 - 1] + s[n / 2]);
}

void hrv_init(struct hrv *h, uint8_t fs)
{
	hrv_beat_cb_t cb = h->cb;
	void *ctx = h->cb_ctx;

	memset(h, 0, sizeof(*h));
	h->enabled = fs == HRV_FS;
	h->cb = cb;
	h->cb_ctx = ctx;
}

double hrv_time_s(const struct hrv *h)
{
	return (double)h->n / HRV_FS;
}

/* add an interval to the open window, pairing it with the previous one in the window */
static void add_interval(struct hrv *h, struct hrv_interval iv)
{
	iv.sq = 0;
	iv.paired = h->prev_in_win && h->prev_clean && iv.clean;
	if (iv.paired) {
		double dd = (double)iv.rr_ms - (double)h->prev_rr;

		iv.sq = dd * dd;
		h->sum_sq += iv.sq;
		h->diffs++;
	}
	h->prev_in_win = true;
	h->prev_rr = iv.rr_ms;
	h->prev_clean = iv.clean;
	h->win_beats++;
	if (h->n_recent == HRV_RECENT) {
		memmove(h->recent, &h->recent[1], (HRV_RECENT - 1) * sizeof(h->recent[0]));
		h->n_recent--;
	}
	h->recent[h->n_recent++] = iv;
}

static void interval(struct hrv *h, double t)
{
	if (h->have_last) {
		float rr = (float)((t - h->last_s) * 1000.0);
		bool in_range = rr >= RR_MIN_MS && rr <= RR_MAX_MS;
		bool good = in_range;

		if (good && h->n_hist >= 3) {
			float last[5];
			int k = h->n_hist < 5 ? h->n_hist : 5;

			for (int i = 0; i < k; i++) {
				last[i] = h->hist[(h->hist_head + 5 - 1 - i) % 5];
			}
			float med = median(last, k);

			good = fabsf(rr - med) <= MAX_DEV * med;
		}
		if (in_range) {
			h->hist[h->hist_head] = rr;
			h->hist_head = (uint8_t)((h->hist_head + 1) % 5);
			h->n_hist = h->n_hist < 5 ? h->n_hist + 1 : 5;
		}
		add_interval(h, (struct hrv_interval){.end_s = t, .rr_ms = rr, .clean = good});
		if (h->cb) {
			h->cb(h->cb_ctx, t, rr, good);
		}
	} else if (h->cb) {
		h->cb(h->cb_ctx, t, NAN, false);
	}
	h->last_s = t;
	h->have_last = true;
}

static void add_slope(struct hrv *h, float s, bool replace)
{
	if (replace && h->n_slopes) {
		h->slopes[(h->slope_head + 7) % 8] = s;
		return;
	}
	h->slopes[h->slope_head] = s;
	h->slope_head = (uint8_t)((h->slope_head + 1) % 8);
	h->n_slopes = h->n_slopes < 8 ? h->n_slopes + 1 : 8;
}

void hrv_push(struct hrv *h, int32_t ppg_counts)
{
	if (!h->enabled) {
		return;
	}
	float x = -(float)ppg_counts;

	if (h->n == 0) {
		for (int s = 0; s < 2; s++) {
			h->z[s][0] = zi[s][0] * x;
			h->z[s][1] = zi[s][1] * x;
		}
	}
	for (int s = 0; s < 2; s++) {
		const float *c = sos[s];
		float y = c[0] * x + h->z[s][0];

		h->z[s][0] = c[1] * x - c[4] * y + h->z[s][1];
		h->z[s][1] = c[2] * x - c[5] * y;
		x = y;
	}
	float dn = h->n == 0 ? 0.0f : x - h->x_prev; /* np.diff with prepend */

	h->x_prev = x;
	h->d[0] = h->d[1];
	h->d[1] = h->d[2];
	h->d[2] = dn;
	h->n++;

	/* the beat waiting out its refractory time becomes final */
	uint32_t i = h->n - 2; /* index of d[1] */

	if (h->pending && i - h->pend_i >= REFRACTORY) {
		h->pending = false;
		h->beats++;
		interval(h, h->pend_s);
	}
	if (h->n < 3 || i < SETTLE) {
		return;
	}
	float a = h->d[0], b = h->d[1], c = h->d[2];

	if (!(b > a && b >= c && b > 0)) {
		return;
	}
	if (h->n_slopes) {
		float recent[8];
		int k = h->n_slopes;

		for (int j = 0; j < k; j++) {
			recent[j] = h->slopes[(h->slope_head + 8 - k + j) % 8];
		}
		if (b < THR_FRAC * median(recent, k)) {
			return;
		}
	}
	float den = a - 2 * b + c;
	float frac = den != 0 ? 0.5f * (a - c) / den : 0.0f;

	frac = frac > 0.5f ? 0.5f : frac < -0.5f ? -0.5f : frac;
	double t = ((double)i + (double)frac) / HRV_FS;

	if (h->pending && i - h->pend_i < REFRACTORY) {
		if (b > h->pend_slope) { /* a steeper slope inside the refractory time replaces it */
			h->pend_i = i;
			h->pend_slope = b;
			h->pend_s = t;
			add_slope(h, b, true);
		}
		return;
	}
	h->pending = true;
	h->pend_i = i;
	h->pend_slope = b;
	h->pend_s = t;
	add_slope(h, b, false);
}

void hrv_flush(struct hrv *h)
{
	if (h->pending) {
		h->pending = false;
		h->beats++;
		interval(h, h->pend_s);
	}
}

struct hrv_window hrv_close(struct hrv *h, double t_end_s)
{
	struct hrv_window w = {0};
	struct hrv_interval carry[HRV_RECENT];
	uint8_t n_carry = 0;

	/* intervals that end after the window were added early: take them back out */
	while (h->n_recent && h->recent[h->n_recent - 1].end_s > t_end_s) {
		const struct hrv_interval *iv = &h->recent[--h->n_recent];

		if (iv->paired) {
			h->sum_sq -= iv->sq;
			h->diffs--;
		}
		h->win_beats--;
		carry[n_carry++] = *iv;
	}
	w.beats = h->win_beats > UINT8_MAX ? UINT8_MAX : (uint8_t)h->win_beats;
	w.n_diffs = h->diffs > UINT8_MAX ? UINT8_MAX : (uint8_t)h->diffs;
	if (h->diffs >= HRV_MIN_DIFFS) {
		double r = sqrt(h->sum_sq / h->diffs) * 10.0;

		w.rmssd_x10 = r > UINT16_MAX ? UINT16_MAX : (uint16_t)lround(r);
	}
	/* open the next window: no pair across windows */
	h->sum_sq = 0;
	h->diffs = 0;
	h->win_beats = 0;
	h->prev_in_win = false;
	h->n_recent = 0;
	while (n_carry) {
		add_interval(h, carry[--n_carry]);
	}
	return w;
}
