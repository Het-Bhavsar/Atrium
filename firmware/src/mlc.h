/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Motion level from the LSM6DSOX machine-learning core, and the activity class
 * built on it (port of app/python/atrium_ml/mlc.py, FEATURES.md #6).
 *
 * On the IMU: the variance of |a| over 39 samples at 12.5 Hz (3.12 s) goes through
 * a 7-node ladder; level k (0..7) lands in MLC0_SRC. The register program is
 * mlc_program, written after the driver's init. It ends with the accelerometer at
 * 12.5 Hz, +-2 g: the MLC needs that rate and full scale while it runs.
 *
 * On the MCU, once per log record: the record's levels plus the step rhythm of
 * the last 100 |a| samples (8 s) give still, walk, run or other. Every threshold
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

#define MLC_FS_MHZ   12500
#define MLC_WINDOW   39  /* accelerometer samples per level */
#define MLC_LEVELS   8
#define MLC_RHYTHM_N 100 /* |a| samples for the step rhythm */

#define MLC_REG_FUNC_CFG_ACCESS 0x01 /* 0x80: embedded-function page */
#define MLC_REG_MLC0_SRC        0x70 /* embedded page */

struct mlc_write {
	uint8_t reg;
	uint8_t val;
};

extern const struct mlc_write mlc_program[];
extern const size_t mlc_program_len;

/* Threshold k (0..6) of the tree in mlc_program, g^2, decoded from its half floats. */
float mlc_threshold(int k);

/* The IMU's ladder on one window variance (g^2): the number of thresholds below it. */
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
