/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Motion level, and the activity class built on it (port of app/python/atrium_ml/mlc.py,
 * FEATURES.md #6).
 *
 * The level: the variance of |a| over one 3.12 s window goes through a 7-node ladder; level k
 * (0..7) is the number of thresholds below it. On the LSM6DSOX this ran in the IMU's
 * machine-learning core at 12.5 Hz (39 samples). The LSM6DSV16BX (U11 since 2026-11-17) has no
 * 12.5 Hz and a different core, so the pod now works it out itself from the samples it already
 * reads (mlc_win_*), with the same thresholds and the same window in seconds: 47 samples at
 * 15 Hz. The reference vectors stay at 12.5 Hz; the lengths come from the rate (mlc_*_len).
 *
 * On the MCU, once per log record: the record's levels plus the step rhythm of the last 8 s of
 * |a| (100 samples at 12.5 Hz, 120 at 15 Hz) give still, walk, run or other. Every threshold
 * is [ASSUMED] until bench test V2. Wellness estimate only. Plain C, no Zephyr.
 */
#ifndef ATRIUM_MLC_H_
#define ATRIUM_MLC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MLC_WINDOW_MS  3120 /* one level */
#define MLC_LEVELS     8
#define MLC_RHYTHM_MS  8000 /* |a| for the step rhythm */
#define MLC_RHYTHM_MAX 128  /* samples: the DFT length */

/* Samples per level window at fs (mHz): round(fs x 3.12 s), 39 at 12.5 Hz, 47 at 15 Hz. */
uint8_t mlc_window_len(uint32_t fs_mhz);

/* Samples of |a| for the step rhythm at fs: round(fs x 8 s), at most MLC_RHYTHM_MAX:
 * 100 at 12.5 Hz, 120 at 15 Hz. */
uint8_t mlc_rhythm_len(uint32_t fs_mhz);

/* One level window, fed |a| in g one sample at a time. */
struct mlc_win {
	float x0; /* the window's first sample: the sums are taken about it, for float precision */
	float s;
	float s2;
	uint8_t n;
	uint8_t len;
};

void mlc_win_init(struct mlc_win *w, uint8_t len);

/* Adds a sample; at the window's end returns true with the level, and starts the next window. */
bool mlc_win_push(struct mlc_win *w, float norm_g, uint8_t *level);

/* Threshold k (0..6), g^2: the half floats of the LSM6DSOX tree, kept bit for bit. */
float mlc_threshold(int k);

/* The ladder on one window variance (g^2): the number of thresholds below it. */
uint8_t mlc_level_of(float variance_g2);

/* Levels seen during one log record. */
struct mlc_record {
	uint8_t hist[MLC_LEVELS];
	uint8_t n;
};

void mlc_record_reset(struct mlc_record *r);
void mlc_record_add(struct mlc_record *r, uint8_t level);
uint8_t mlc_record_max(const struct mlc_record *r);
/* np.median of the levels, truncated to an integer (as int() in the reference). 0 when empty. */
uint8_t mlc_record_median(const struct mlc_record *r);

/*
 * Step rhythm from |a| in g (n samples at fs): mean removed, Hann window, 128-point
 * real DFT, peak in 1..3.5 Hz with parabolic refinement. share is the power within
 * 0.15 Hz of the peak over the 1..3.5 Hz power. False when the band has no power.
 */
bool mlc_step_rhythm(const float *norm_g, size_t n, float fs, float *spm, float *share);

enum mlc_class {
	MLC_STILL = 0,
	MLC_WALK = 1,
	MLC_RUN = 2,
	MLC_OTHER = 3,
};

enum mlc_class mlc_activity_class(const struct mlc_record *r, bool rhythm_ok, float spm,
				  float share);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_MLC_H_ */
