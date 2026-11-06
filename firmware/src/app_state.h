/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Latest readings and states, shared by the main loop, the sensor callbacks
 * and the BLE services. Readers take a copy.
 */
#ifndef ATRIUM_APP_STATE_H_
#define ATRIUM_APP_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include "hrv.h"
#include "power_sm.h"

struct app_status {
	struct power_sm_out power;
	uint16_t vbat_mv;
	uint16_t vbus_mv;
	uint8_t soc;           /* soc_est.c */
	bool skin_valid;       /* TH1, skin side */
	int16_t skin_cc;
	bool top_valid;        /* TH4, top side */
	int16_t top_cc;
	bool chrg_n_low;
	uint8_t bpm;
	uint8_t bpm_conf;
	uint16_t activity_mg;
	bool moving;
	bool still;
	bool on_skin; /* wear detection (wear.c): worn and off the dock */
	bool pulse_ir; /* pulse from IR: green has no headroom */
	struct hrv_window hrv; /* last closed log interval */
	int16_t pitch_deg;     /* last closed log interval; INT16_MIN unknown */
	int16_t roll_deg;
	/* last closed log record: MLC motion (mlc.h) */
	uint8_t mlc_max;
	uint8_t mlc_class;
	uint8_t steps_pm;
	/* last oxygen-trend burst (spo2.h); ox_seq counts bursts */
	uint16_t ox_seq;
	uint32_t ox_time;
	float ox_r;          /* NAN when no window passed */
	float ox_conf;
	uint8_t ox_good;
};

struct app_settings {
	uint8_t mode;          /* enum pod_mode, user choice */
	uint16_t sleep_start;  /* minutes after local midnight */
	uint16_t sleep_end;
	int16_t utc_offset_min;
	uint16_t log_interval_s;
};

#define APP_SETTINGS_DEFAULT                                                      \
	{                                                                         \
		.mode = MODE_SMART, .sleep_start = 23 * 60, .sleep_end = 7 * 60,   \
		.utc_offset_min = 0, .log_interval_s = 15,                         \
	}

void app_status_get(struct app_status *out);
void app_status_update(void (*fn)(struct app_status *s, void *arg), void *arg);

void app_settings_get(struct app_settings *out);
void app_settings_set(const struct app_settings *in);

/* Unix seconds once the app has set the time, else uptime with bit 31 set. */
uint32_t app_time_now(void);
void app_time_set(uint32_t unix_s);
bool app_time_is_set(void);

/* True inside the sleep window (needs the time to be set). */
bool app_is_night(const struct app_settings *s);

#endif /* ATRIUM_APP_STATE_H_ */
