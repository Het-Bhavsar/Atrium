/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wear detection (docs/FEATURES.md #18; port of app/python/atrium_ml/wear.py update()),
 * decided once a second from three cues:
 *  - proximity: DC per mA of the pulse LED (green, or IR after the low-battery swap),
 *    relative to the user's own on-skin level for that LED. Open air reflects almost
 *    nothing. One reference per LED, each learned only while worn with a pulse present
 *    (its first such minute); both are cleared on the dock and at off-body. Until the
 *    active LED has one, the proximity cue is skipped;
 *  - pulse: pulse presence from the unweighted spectrum (pulse.h), never the HR confidence;
 *  - warmth: the skin NTC TH1 only (TH4 sits next to the nRF). Off the head the pod cools.
 * Worn -> off-body: dark for 4 s; or no pulse for 60 s while still and cool (below 30 C or
 * falling faster than 0.1 C/min); or no pulse for 300 s while below 32 C. Without a
 * reference only the no-pulse paths apply.
 * Off-body -> worn: lit for 2 s, then a pulse within 15 s; without a reference, a pulse
 * alone. During exercise motion masks the pulse, but the pod stays warm and lit, so it
 * stays worn. Thresholds are the same for every LED and [ASSUMED] until the bench sets
 * them (V7 runs each case on green and on forced IR). Plain C, no Zephyr.
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
	uint16_t learn_s;       /* pulse-present seconds that set an on-skin reference */
};

#define WEAR_CFG_DEFAULT                                                          \
	{                                                                         \
		.dc_off = 0.15f, .dc_on = 0.40f, .dark_s = 4, .on_s = 2,           \
		.confirm_s = 15, .still_mg = 15.0f, .no_pulse_s = 60,              \
		.no_pulse_long_s = 300, .cool_c = 30.0f, .cool_rate = -0.10f,      \
		.warm_c = 32.0f, .learn_s = 60,                                    \
	}

#define WEAR_TEMP_HIST 61
#define WEAR_LEDS      3 /* enum led_id: green, IR, red */

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
	float dc_ref[WEAR_LEDS]; /* NAN: none yet */
	double learn_sum;
	uint16_t learn_n;
	uint8_t learn_led;
};

/* Starts worn with no references, so the full measurement runs and decides. */
void wear_init(struct wear *w, const struct wear_cfg *cfg);

/* On the dock: a new wear starts, and both references are learned again. */
void wear_restart(struct wear *w);

/* The active LED has a reference. */
bool wear_has_ref(const struct wear *w, uint8_t led);

/*
 * Once a second while undocked. led: the pulse LED (enum led_id). dc_per_ma: its DC / LED
 * mA (dc_valid false when there was no sample). pulse: pulse presence. motion_mg:
 * accelerometer RMS. temp_c: TH1.
 */
bool wear_update(struct wear *w, uint8_t led, bool dc_valid, float dc_per_ma, bool pulse,
		 float motion_mg, bool temp_valid, float temp_c);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_WEAR_H_ */
