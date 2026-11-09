/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * One 30 s log epoch: per-second inputs from the main loop, closed into a log record with
 * the sensors' per-epoch results (HRV, motion, posture, activity, burst). Plain C, no Zephyr.
 *
 *  - pulse: mean of the seconds with a reported pulse (worn, valid track); confidence the
 *    mean of the same seconds;
 *  - on skin: worn for more than half the epoch; pulse from IR, docked and charging: any
 *    second;
 *  - steps: the step rhythm x 30 s, in walk and run epochs only [ASSUMED until the bench
 *    compares it with counted steps].
 * An epoch spent wholly on the dock is not logged: the log has a gap there.
 */
#ifndef ATRIUM_EPOCH_H_
#define ATRIUM_EPOCH_H_

#include <stdbool.h>
#include <stdint.h>

#include "hrv.h"
#include "log_record.h"

#ifdef __cplusplus
extern "C" {
#endif

struct epoch_second {
	bool docked;
	bool charging;
	bool on_skin;
	bool pulse_ir;
	bool pulse_present;
	uint8_t bpm;  /* 0 = no pulse reported this second */
	float conf;   /* 0..1 */
};

struct epoch_close_in {
	struct hrv_window hrv;
	uint8_t mlc_max;
	uint8_t cls;        /* enum mlc_class */
	uint8_t steps_pm;
	int16_t pitch_deg;  /* INT16_MIN unknown */
	int16_t roll_deg;
	uint16_t activity_mg;
	bool skin_valid;
	int32_t skin_cc;
	bool ox;            /* a burst ended in this epoch */
	float ox_r;
	float ox_conf;
};

struct epoch {
	uint32_t t0;
	uint8_t n;          /* seconds seen */
	uint8_t undocked;
	uint8_t worn;
	uint8_t pulse_s;
	uint8_t bpm_n;
	uint32_t bpm_sum;
	float conf_sum;
	bool docked, charging, pulse_ir;
};

void epoch_open(struct epoch *e, uint32_t t0);

/*
 * Start time of the next epoch: the previous one + 30 s while the clock runs on (within
 * 2 s, same time base), else `now` (the phone set the clock, or there was no previous
 * epoch). Keeps consecutive epochs exactly 30 s apart, so the log opens no needless blocks.
 */
uint32_t epoch_next_t0(uint32_t prev_t0, bool have_prev, uint32_t now);
void epoch_second(struct epoch *e, const struct epoch_second *s);

/* The record for the epoch; false when it was spent on the dock (not logged). */
bool epoch_close(const struct epoch *e, const struct epoch_close_in *in, struct log_record *out);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_EPOCH_H_ */
