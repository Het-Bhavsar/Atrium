/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Beat-to-beat intervals and RMSSD (port of app/python/atrium_ml/hrv.py, "max-slope PPI",
 * FEATURES.md #2). Wellness estimate; use only still windows.
 *
 * PPG band-passed 0.5-5 Hz at 25 Hz. The fiducial is the maximum of the first difference on
 * each upstroke, at least 0.4 x the median of the last 8 beat slopes, and 0.33 s after the
 * previous beat; a steeper slope inside that time replaces it. Timing is refined by a
 * parabola through the slope maximum. Intervals count if they are 300 to 2000 ms and within
 * 15 % of the median of the last 5 in-range intervals. A successive difference counts when
 * both intervals are clean and fall in the same log window (by end time). Plain C, no Zephyr.
 */
#ifndef ATRIUM_HRV_H_
#define ATRIUM_HRV_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HRV_FS        25
#define HRV_MIN_DIFFS 5 /* fewer clean differences in a window: RMSSD 0 */
#define HRV_RECENT    4 /* intervals that may still belong to the next window */

struct hrv_window {
	uint16_t rmssd_x10; /* 0.1 ms, 0 when there are fewer than HRV_MIN_DIFFS */
	uint8_t n_diffs;    /* clean successive differences in the window */
	uint8_t beats;      /* beats ending an interval in the window */
};

struct hrv_interval {
	double end_s;
	float rr_ms;
	bool clean;
	bool paired; /* it added a successive difference ... */
	double sq;   /* ... of this square */
};

/* Optional hook for every finalized beat (rr_ms NAN for the first). */
typedef void (*hrv_beat_cb_t)(void *ctx, double beat_s, float rr_ms, bool clean);

struct hrv {
	bool enabled;
	uint32_t n;
	float z[2][2];
	float x_prev;
	float d[3];          /* d[i-1], d[i], d[i+1] */
	float slopes[8];
	uint8_t n_slopes;
	uint8_t slope_head;
	bool pending;        /* a detected beat that may still be replaced */
	uint32_t pend_i;
	float pend_slope;
	double pend_s;
	bool have_last;
	double last_s;
	float hist[5];
	uint8_t n_hist;
	uint8_t hist_head;
	struct hrv_interval recent[HRV_RECENT]; /* last intervals added to the open window */
	uint8_t n_recent;
	bool prev_in_win;
	float prev_rr;
	bool prev_clean;
	double sum_sq;
	uint16_t diffs;
	uint16_t win_beats;
	uint16_t beats;
	hrv_beat_cb_t cb;
	void *cb_ctx;
};

/* fs other than 25 Hz disables the estimate. */
void hrv_init(struct hrv *h, uint8_t fs);

/* One PPG sample, as the AFE gives it (intensity). */
void hrv_push(struct hrv *h, int32_t ppg_counts);

/*
 * Close the window ending at t_end_s (seconds since the first sample). Intervals ending
 * later stay for the next window. Call it at least 0.4 s after t_end_s so every beat up to
 * t_end_s is final.
 */
struct hrv_window hrv_close(struct hrv *h, double t_end_s);

/* End of signal: the beat waiting out its refractory time becomes final. */
void hrv_flush(struct hrv *h);

/* Seconds of signal seen. */
double hrv_time_s(const struct hrv *h);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_HRV_H_ */
