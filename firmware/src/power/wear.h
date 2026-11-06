/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wear detection (docs/FEATURES.md #18; port of app/python/atrium_ml/wear.py update()),
 * decided once a second from three cues:
 *  - proximity: green DC per mA, relative to the user's own on-skin level (learned from the
 *    first pulse-present minute of each wear). Open air reflects almost nothing;
 *  - pulse: pulse presence from the unweighted spectrum (pulse.h), never the HR confidence;
 *  - warmth: the skin NTC TH1. Off the head the pod cools.
 * Worn -> off-body: dark for 4 s; or no pulse for 60 s while still and cool (below 30 C or
 * falling faster than 0.1 C/min); or no pulse for 300 s while below 32 C.
 * Off-body -> worn: lit for 2 s, then a pulse within 15 s. During exercise motion masks the
 * pulse, but the pod stays warm and lit, so it stays worn.
 * Thresholds are [ASSUMED] until the bench sets them. Plain C, no Zephyr.
 */
#ifndef ATRIUM_WEAR_H_
#define ATRIUM_WEAR_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wear_cfg {
	float dc_off;           /* relative DC below this: dark */
	float dc_on;            /* relative DC above this: lit */
	uint16_t dark_s;
	uint16_t on_s;
	uint16_t confirm_s;
	float still_mg;
	uint16_t no_pulse_s;
	uint16_t no_pulse_long_s;
	float cool_c;
	float cool_rate;        /* C per minute */
	float warm_c;
	float dc_ref_default;   /* counts per mA before the first learned minute [ASSUMED] */
	uint16_t learn_s;       /* pulse-present seconds that set the on-skin reference */
};

#define WEAR_CFG_DEFAULT                                                          \
	{                                                                         \
		.dc_off = 0.15f, .dc_on = 0.40f, .dark_s = 4, .on_s = 2,           \
		.confirm_s = 15, .still_mg = 15.0f, .no_pulse_s = 60,              \
		.no_pulse_long_s = 300, .cool_c = 30.0f, .cool_rate = -0.10f,      \
		.warm_c = 32.0f, .dc_ref_default = 10000.0f, .learn_s = 60,        \
	}

#define WEAR_TEMP_HIST 61

struct wear {
	struct wear_cfg cfg;
	bool worn;
	uint16_t dark;
	uint16_t lit;
	uint16_t since_pulse;
	int16_t pending; /* seconds since lit while waiting for a pulse, -1 when not waiting */
	float temp[WEAR_TEMP_HIST];
	uint8_t t_n;
	uint8_t t_head;
	float dc_ref;
	double learn_sum;
	uint16_t learn_n;
};

/* Starts worn, so the full measurement runs and decides. The learned reference is kept. */
void wear_init(struct wear *w, const struct wear_cfg *cfg);

/* A new wear (lifted from the dock): learn the on-skin reference again. */
void wear_restart(struct wear *w);

/*
 * Once a second while undocked. dc_per_ma: green DC / LED mA (dc_valid false when there was
 * no green sample). pulse: pulse presence. motion_mg: accelerometer RMS. temp_c: TH1.
 */
bool wear_update(struct wear *w, bool dc_valid, float dc_per_ma, bool pulse, float motion_mg,
		 bool temp_valid, float temp_c);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_WEAR_H_ */
