/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pulse.h"

#include <math.h>
#include <string.h>

/* HrConfig in atrium_ml/hr.py */
#define F_LO          0.6f
#define F_HI          3.5f
#define MOTION_MG     20.0f /* [ASSUMED] band-passed accelerometer RMS that turns on the weighting */
#define MASK_FLOOR    0.001f
#define TRACK_SIGMA   12.0f
#define HALF_RATIO    0.5f
#define SLEW_BPM      6.0f
#define CONF_MIN      0.12f /* [ASSUMED] */
#define LOST_AFTER    10
#define MASK_POWER    0.25f
#define MOVING_F_LO   0.9f
#define SWITCH_RATIO  2.0f
#define SWITCH_AFTER  6
#define SWITCH_BPM    8.0f
/* atrium_ml/wear.py WearConfig */
#define PULSE_CONF    0.5f
#define PULSE_RUN_S   5
#define PULSE_TOL_BPM 4.0f

#define PI_F 3.14159265358979f
#define DF   ((float)PULSE_FS / PULSE_NFFT)
#define B_LO 6  /* floor(F_LO / DF) */
#define B_HI 37 /* ceil(F_HI / DF) + 1 */
#define B_MV 9  /* floor(MOVING_F_LO / DF) */

/* Butterworth band-pass 0.6-3.5 Hz at 25 Hz, order 2 (two biquads), from
 * scipy.signal.butter(2, [0.6, 3.5], 'bandpass', fs=25, output='sos'), and sosfilt_zi. */
static const float sos[2][6] = {
	{0.08635926487432913f, 0.17271852974865826f, 0.08635926487432913f, 1.0f,
	 -1.0035348883619846f, 0.43345168340791385f},
	{1.0f, -2.0f, 1.0f, 1.0f, -1.8037627909882736f, 0.8297888456922199f},
};
static const float zi[2][2] = {
	{0.7171382106323264f, -0.2619180684980398f},
	{-0.8034974755066593f, 0.8034974755066587f},
};

static float hann[PULSE_WIN];
static float tw_re[PULSE_NFFT / 2], tw_im[PULSE_NFFT / 2];
static bool tables;

static void make_tables(void)
{
	for (int i = 0; i < PULSE_WIN; i++) {
		hann[i] = 0.5f - 0.5f * cosf(2.0f * PI_F * i / (PULSE_WIN - 1));
	}
	for (int i = 0; i < PULSE_NFFT / 2; i++) {
		tw_re[i] = cosf(-2.0f * PI_F * i / PULSE_NFFT);
		tw_im[i] = sinf(-2.0f * PI_F * i / PULSE_NFFT);
	}
	tables = true;
}

static void bq_prime(struct pulse_biquad2 *f, float x0)
{
	for (int s = 0; s < 2; s++) {
		f->z[s][0] = zi[s][0] * x0;
		f->z[s][1] = zi[s][1] * x0;
	}
}

/* direct form II transposed, as scipy.signal.sosfilt */
static float bq_run(struct pulse_biquad2 *f, float x)
{
	for (int s = 0; s < 2; s++) {
		const float *c = sos[s];
		float y = c[0] * x + f->z[s][0];

		f->z[s][0] = c[1] * x - c[4] * y + f->z[s][1];
		f->z[s][1] = c[2] * x - c[5] * y;
		x = y;
	}
	return x;
}

/* power spectrum |rfft(seg * hann, 256)|^2 of the ring buffer's last 200 samples, mean removed */
static void power(const float *ring, uint16_t head, float out[PULSE_NBIN])
{
	static float re[PULSE_NFFT], im[PULSE_NFFT];
	float mean = 0;

	for (int i = 0; i < PULSE_WIN; i++) {
		mean += ring[(head + i) % PULSE_WIN];
	}
	mean /= PULSE_WIN;
	for (int i = 0; i < PULSE_NFFT; i++) {
		re[i] = i < PULSE_WIN ? (ring[(head + i) % PULSE_WIN] - mean) * hann[i] : 0.0f;
		im[i] = 0.0f;
	}
	/* iterative radix-2 FFT */
	for (int i = 1, j = 0; i < PULSE_NFFT; i++) {
		int bit = PULSE_NFFT >> 1;

		for (; j & bit; bit >>= 1) {
			j ^= bit;
		}
		j ^= bit;
		if (i < j) {
			float t = re[i];

			re[i] = re[j];
			re[j] = t;
		}
	}
	for (int len = 2; len <= PULSE_NFFT; len <<= 1) {
		int step = PULSE_NFFT / len;

		for (int i = 0; i < PULSE_NFFT; i += len) {
			for (int k = 0; k < len / 2; k++) {
				float wr = tw_re[k * step], wi = tw_im[k * step];
				int u = i + k, v = i + k + len / 2;
				float xr = re[v] * wr - im[v] * wi;
				float xi = re[v] * wi + im[v] * wr;

				re[v] = re[u] - xr;
				im[v] = im[u] - xi;
				re[u] += xr;
				im[u] += xi;
			}
		}
	}
	for (int i = 0; i < PULSE_NBIN; i++) {
		out[i] = re[i] * re[i] + im[i] * im[i];
	}
}

static float parabolic(const float *y, int n, int i)
{
	if (i <= 0 || i >= n - 1) {
		return 0.0f;
	}
	float a = y[i - 1], b = y[i], c = y[i + 1];
	float den = a - 2 * b + c;

	if (den == 0.0f) {
		return 0.0f;
	}
	float f = 0.5f * (a - c) / den;

	return f > 0.5f ? 0.5f : f < -0.5f ? -0.5f : f;
}

static float maxof(const float *p, int lo, int hi)
{
	float m = p[lo];

	for (int i = lo + 1; i < hi; i++) {
		m = p[i] > m ? p[i] : m;
	}
	return m;
}

/* One tracker step on a spectrum (atrium_ml/hr.py spectral_hr loop body). */
static void track_step(struct pulse_track *t, const float *pw, int lo_s, float *conf_out,
		       float *peak_out)
{
	static int cand[PULSE_NBIN]; /* static: keeps the work-queue stack small */
	int nc = 0;

	for (int i = (lo_s > 1 ? lo_s : 1); i < (B_HI < PULSE_NBIN - 1 ? B_HI : PULSE_NBIN - 1); i++) {
		if (pw[i] >= pw[i - 1] && pw[i] > pw[i + 1]) {
			cand[nc++] = i;
		}
	}
	if (nc == 0) {
		*conf_out = 0.0f;
		*peak_out = NAN;
		t->lost++;
		return;
	}
	float pmax = 0;
	int top = cand[0];

	for (int c = 0; c < nc; c++) {
		if (pw[cand[c]] > pmax) {
			pmax = pw[cand[c]];
			top = cand[c];
		}
	}
	int k;

	if (t->have_est && t->lost < LOST_AFTER) {
		float sigma = TRACK_SIGMA * (1.0f + 0.3f * t->lost);
		float best = -1.0f;

		k = cand[0];
		for (int c = 0; c < nc; c++) {
			float f = (cand[c] + parabolic(pw, PULSE_NBIN, cand[c])) * DF * 60.0f;
			float z = (f - t->est) / sigma;
			float score = pw[cand[c]] / pmax * expf(-0.5f * z * z);

			if (score > best) {
				best = score;
				k = cand[c];
			}
		}
		/* a different peak that stays much stronger than the tracked one takes over */
		float f_top = top * DF * 60.0f;

		if (fabsf(f_top - t->est) > 2 * SWITCH_BPM && pw[top] >= SWITCH_RATIO * pw[k]) {
			t->alt_n = (t->have_alt && fabsf(f_top - t->alt) <= SWITCH_BPM) ? t->alt_n + 1 : 1;
			t->alt = f_top;
			t->have_alt = true;
			if (t->alt_n >= SWITCH_AFTER) {
				k = top;
				t->have_est = false;
				t->have_alt = false;
				t->alt_n = 0;
			}
		} else {
			t->have_alt = false;
			t->alt_n = 0;
		}
	} else {
		k = top;
		/* prefer the fundamental: a peak near half the frequency with half the power */
		int h = -1;

		for (int c = 0; c < nc; c++) {
			if (fabsf(cand[c] - k / 2.0f) <= 1.0f && (h < 0 || pw[cand[c]] > pw[h])) {
				h = cand[c];
			}
		}
		if (h >= 0 && pw[h] >= HALF_RATIO * pw[k] && h * DF * 60.0f >= 40.0f) {
			k = h;
		}
	}
	float f = (k + parabolic(pw, PULSE_NBIN, k)) * DF;
	float bpm = f * 60.0f;
	float near = 0, total = 0;

	for (int i = (k - 1 > 0 ? k - 1 : 0); i < k + 2; i++) {
		near += pw[i];
	}
	int k2 = (int)rintf(2.0f * f / DF); /* round half to even, as numpy */

	if (k2 + 1 < B_HI) {
		near += pw[k2 - 1] + pw[k2] + pw[k2 + 1];
	}
	for (int i = B_LO; i < B_HI; i++) {
		total += pw[i];
	}
	float conf = near / (total + 1e-12f);

	conf = conf > 1.0f ? 1.0f : conf;
	if (conf >= CONF_MIN) {
		if (!t->have_est || t->lost >= LOST_AFTER) {
			t->est = bpm;
		} else {
			float d = bpm - t->est;

			d = d > SLEW_BPM ? SLEW_BPM : d < -SLEW_BPM ? -SLEW_BPM : d;
			t->est += d;
		}
		t->have_est = true;
		t->lost = 0;
	} else {
		t->lost++;
	}
	*conf_out = conf;
	*peak_out = bpm;
}

void pulse_init(struct pulse_est *p, uint8_t fs)
{
	memset(p, 0, sizeof(*p));
	if (!tables) {
		make_tables();
	}
	p->enabled = fs == PULSE_FS;
	p->prev_raw_peak = NAN;
}

static void frame(struct pulse_est *p, bool have_acc)
{
	static float px[PULSE_NBIN], m[PULSE_NBIN], tmp[PULSE_NBIN], pw[PULSE_NBIN];
	struct pulse_out *o = &p->out;
	float mot = 0;

	power(p->x, p->head, px);
	o->acc_peak_bpm = NAN;
	bool moving = false;

	if (have_acc) {
		float mean[3] = {0, 0, 0};

		for (int k = 0; k < 3; k++) {
			for (int i = 0; i < PULSE_WIN; i++) {
				mean[k] += p->a[k][i];
			}
			mean[k] /= PULSE_WIN;
		}
		for (int i = 0; i < PULSE_WIN; i++) {
			for (int k = 0; k < 3; k++) {
				float d = p->a[k][i] - mean[k];

				mot += d * d;
			}
		}
		mot = sqrtf(mot / PULSE_WIN);
		moving = mot > MOTION_MG;
	}
	o->motion_mg = mot;
	if (moving) {
		memset(m, 0, sizeof(m));
		for (int k = 0; k < 3; k++) {
			power(p->a[k], p->head, tmp);
			for (int i = 0; i < PULSE_NBIN; i++) {
				m[i] += tmp[i];
			}
		}
		int am = B_LO;

		for (int i = B_LO; i < B_HI; i++) {
			am = m[i] > m[am] ? i : am;
		}
		o->acc_peak_bpm = am * DF * 60.0f;
	}

	/* unweighted spectrum: pulse presence (wear.py) */
	track_step(&p->raw_trk, px, B_LO, &o->raw_conf, &o->raw_peak_bpm);
	bool steady = !isnan(o->raw_peak_bpm) && !isnan(p->prev_raw_peak) &&
		      fabsf(o->raw_peak_bpm - p->prev_raw_peak) <= PULSE_TOL_BPM;
	bool at_motion = false;

	if (!isnan(o->acc_peak_bpm) && !isnan(o->raw_peak_bpm)) {
		static const float h[3] = {0.5f, 1.0f, 2.0f};

		for (int i = 0; i < 3; i++) {
			at_motion |= fabsf(o->raw_peak_bpm - h[i] * o->acc_peak_bpm) <= PULSE_TOL_BPM;
		}
	}
	p->present_run = (o->raw_conf >= PULSE_CONF && steady && !at_motion) ? p->present_run + 1 : 0;
	o->pulse_present = p->present_run >= PULSE_RUN_S;
	p->prev_raw_peak = o->raw_peak_bpm;

	/* motion-weighted spectrum: the HR output */
	const float *use = px;
	int lo_s = B_LO;

	if (moving) {
		for (int i = 0; i < PULSE_NBIN; i++) {
			float l = m[i == 0 ? PULSE_NBIN - 1 : i - 1], r = m[i == PULSE_NBIN - 1 ? 0 : i + 1];
			float w = l > r ? l : r;

			tmp[i] = m[i] > w ? m[i] : w; /* widen by one bin (numpy roll wraps) */
		}
		float mmax = maxof(tmp, B_LO, B_HI) + 1e-12f;
		float pmax = maxof(px, B_LO, B_HI) + 1e-12f;

		for (int i = 0; i < PULSE_NBIN; i++) {
			float mn = powf(tmp[i] / mmax, MASK_POWER);
			float pn = px[i] / pmax;
			float v = pn - mn;

			pw[i] = v > MASK_FLOOR * pn ? v : MASK_FLOOR * pn;
		}
		use = pw;
		lo_s = B_MV;
	}
	track_step(&p->trk, use, lo_s, &o->conf, &o->peak_bpm);
	o->bpm = p->trk.have_est ? p->trk.est : NAN;
	o->valid = o->conf >= CONF_MIN;
	p->have_out = true;
}

bool pulse_push(struct pulse_est *p, int32_t ppg_counts, const int16_t acc_mg[3])
{
	if (!p->enabled) {
		return false;
	}
	float x = -(float)ppg_counts; /* blood volume up, intensity down */

	if (!p->primed) {
		bq_prime(&p->fx, x);
		for (int k = 0; k < 3; k++) {
			bq_prime(&p->fa[k], acc_mg ? acc_mg[k] : 0.0f);
		}
		p->primed = true;
	}
	p->x[p->head] = bq_run(&p->fx, x);
	for (int k = 0; k < 3; k++) {
		p->a[k][p->head] = bq_run(&p->fa[k], acc_mg ? acc_mg[k] : 0.0f);
	}
	p->head = (uint16_t)((p->head + 1) % PULSE_WIN);
	p->n++;
	if (p->n >= PULSE_WIN && (p->n - PULSE_WIN) % PULSE_FS == 0) {
		frame(p, acc_mg != NULL);
		return true;
	}
	return false;
}

bool pulse_get(const struct pulse_est *p, struct pulse_out *out)
{
	if (!p->have_out) {
		return false;
	}
	*out = p->out;
	return true;
}
