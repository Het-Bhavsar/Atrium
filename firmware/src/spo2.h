/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Oxygen trend from the red and IR LEDs (port of app/python/atrium_ml/spo2.py,
 * FEATURES.md #8). The pod reports a relative red/IR ratio R per 30 s burst with a
 * confidence: no calibration and no percentage on the pod. Research and wellness
 * use only.
 *
 * Per burst at 25 Hz: drop the first 4 s; then 4 s windows stepped by 2 s (12 of
 * them). Per window, DC = mean of the raw counts, AC = RMS of the 0.5-4 Hz band-pass
 * (two biquads per channel, primed at the burst start), R = (AC/DC)red / (AC/DC)IR.
 * A window is good when red and IR agree (correlation), the IR perfusion is high
 * enough, the head is still, the pulse is present and neither channel is near the
 * ADC limits. Burst R = median of the good windows; confidence = good share x
 * (1 - IQR/median / 0.2); 0 below 4 good windows. Every gate is [ASSUMED].
 * Running sums per 2 s block, no sample buffers: about 0.3 KB. Plain C, no Zephyr.
 */
#ifndef ATRIUM_SPO2_H_
#define ATRIUM_SPO2_H_

#include <stdbool.h>
#include <stdint.h>

#include "modes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPO2_FS       25
#define SPO2_SAMPLES  (30 * SPO2_FS)
#define SPO2_BLOCK    (2 * SPO2_FS)
#define SPO2_SETTLE   (4 * SPO2_FS)
#define SPO2_WINDOWS  ((SPO2_SAMPLES - SPO2_SETTLE) / SPO2_BLOCK - 1)

/* burst LED currents on top of the mode's green slot, mA */
#define SPO2_IR_MA  10
#define SPO2_RED_MA 10

struct spo2_cfg {
	float min_corr;
	float min_pi;
	float max_motion_mg;
	float adc_full;
	float adc_margin;
	uint8_t min_good;
	float max_spread;
};

#define SPO2_CFG_DEFAULT                                                          \
	{                                                                         \
		.min_corr = 0.8f, .min_pi = 2e-4f, .max_motion_mg = 15.0f,         \
		.adc_full = 524287.0f, .adc_margin = 0.05f, .min_good = 4,         \
		.max_spread = 0.2f,                                                \
	}

struct spo2_block {
	double sr, si, srr, sii, sri; /* band-passed red and IR */
	double dr, di;                /* raw counts */
	int32_t rmin, rmax, imin, imax;
	float motion_max;
	bool pulse_all;
};

struct spo2_window {
	float t_s; /* window end, from the burst start */
	float r;   /* NAN when undefined */
	float corr;
	float pi_ir;
	bool good;
};

struct spo2 {
	struct spo2_cfg cfg;
	double zr[2][2], zi[2][2]; /* biquad states */
	int32_t r0, i0;            /* first sample: the filters start in steady state on it */
	uint16_t n;
	struct spo2_block prev, cur;
	/* per-second gates: the latest values, merged into the open second */
	bool have_gate, sec_open;
	float gate_motion, sec_motion;
	bool gate_pulse, sec_pulse;
	uint8_t n_win;
	struct spo2_window win[SPO2_WINDOWS];
};

struct spo2_out {
	float r;          /* NAN when fewer than min_good windows pass */
	float confidence; /* 0..1 */
	uint8_t n_good;
	uint8_t n_windows;
};

void spo2_init(struct spo2 *s, const struct spo2_cfg *cfg);

/* Motion (mg) and pulse presence, about once a second: they gate the second being sampled. */
void spo2_gate(struct spo2 *s, float motion_mg, bool pulse_ok);

/* One red and IR sample pair (raw counts). Ignored once the burst is complete. */
void spo2_push(struct spo2 *s, int32_t red, int32_t ir);

bool spo2_done(const struct spo2 *s);

/* The burst result from the windows so far. */
void spo2_result(const struct spo2 *s, struct spo2_out *out);

/* Burst timing: sleep every 5 min, smart every 30 min, only after 60 s of MLC level 0
 * with a pulse present; otherwise retry each minute. None in other modes. */
struct spo2_sched {
	enum pod_mode mode;
	uint32_t next_s;
	uint32_t moved_s; /* last non-zero MLC level */
	uint32_t level_s; /* last MLC level of any value */
	bool have_level;
};

#define SPO2_PERIOD_SLEEP_S 300
#define SPO2_PERIOD_SMART_S 1800
#define SPO2_RETRY_S        60
#define SPO2_STILL_S        60

void spo2_sched_init(struct spo2_sched *q, uint32_t now_s);
void spo2_sched_level(struct spo2_sched *q, uint32_t now_s, uint8_t level);

/* Once a second. can: the optics may run a burst now (worn, green headroom, not docked). */
bool spo2_sched_tick(struct spo2_sched *q, uint32_t now_s, enum pod_mode mode, bool can,
		     bool pulse_present);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_SPO2_H_ */
