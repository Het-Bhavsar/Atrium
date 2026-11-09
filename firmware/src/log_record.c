/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Arithmetic in double with round-half-even (rint), as Python's round() in logrec.py, so
 * the two agree byte for byte.
 */
#include "log_record.h"

#include <math.h>

static uint8_t u8(double v, double lo, double hi)
{
	double r = rint(v);

	return (uint8_t)(r < lo ? lo : r > hi ? hi : r);
}

static int8_t angle(int16_t deg, double step)
{
	if (deg == LOG_ANGLE_UNKNOWN) {
		return INT8_MIN;
	}
	double r = rint((double)deg / step);

	return (int8_t)(r < -90 ? -90 : r > 90 ? 90 : r);
}

void log_record_pack(const struct log_record *r, uint8_t out[LOG_RECORD_SIZE])
{
	uint8_t b2 = 0;

	if (r->rmssd_ms > 0.0f) {
		b2 = u8(r->rmssd_ms, 0, 254);
		b2 = b2 ? b2 : 1;
	}
	out[0] = u8(r->bpm, 0, 255);
	out[1] = (uint8_t)(u8((double)r->conf * 15, 0, 15) | (r->on_skin ? 0x10 : 0) |
			   (r->pulse_ir ? 0x20 : 0) | (r->docked ? 0x40 : 0) | (r->charging ? 0x80 : 0));
	out[2] = b2;
	out[3] = (uint8_t)((r->n_diffs > 63 ? 63 : r->n_diffs) | ((r->cls & 0x03) << 6));
	out[4] = u8(r->activity_mg, 0, 255);
	out[5] = (uint8_t)((r->mlc_max > 7 ? 7 : r->mlc_max) | ((r->pulse_s > 30 ? 30 : r->pulse_s) << 3));
	if (r->skin_cc == LOG_SKIN_UNKNOWN) {
		out[6] = 0;
	} else {
		double c = r->skin_cc / 100.0;

		out[6] = c < 25.05 ? 0 : u8((c - 25.0) / 0.1, 1, 254);
	}
	out[7] = (uint8_t)(r->steps > 255 ? 255 : r->steps);
	out[8] = (uint8_t)angle(r->pitch_deg, 1.0);
	out[9] = (uint8_t)angle(r->roll_deg, 2.0);
	out[10] = r->ox_r > 0.0f ? u8((double)r->ox_r * 100, 1, 255) : 0;
	out[11] = out[10] ? u8((double)r->ox_conf * 15, 0, 15) : 0;
}

void log_record_unpack(const uint8_t in[LOG_RECORD_SIZE], struct log_record *r)
{
	r->bpm = in[0];
	r->conf = (float)(in[1] & 0x0F) / 15.0f;
	r->on_skin = in[1] & 0x10;
	r->pulse_ir = in[1] & 0x20;
	r->docked = in[1] & 0x40;
	r->charging = in[1] & 0x80;
	r->rmssd_ms = in[2];
	r->n_diffs = in[3] & 0x3F;
	r->cls = in[3] >> 6;
	r->activity_mg = in[4];
	r->mlc_max = in[5] & 0x07;
	r->pulse_s = in[5] >> 3;
	r->skin_cc = in[6] ? (int16_t)(2500 + 10 * in[6]) : LOG_SKIN_UNKNOWN;
	r->steps = in[7];
	r->pitch_deg = (int8_t)in[8] == INT8_MIN ? LOG_ANGLE_UNKNOWN : (int8_t)in[8];
	r->roll_deg = (int8_t)in[9] == INT8_MIN ? LOG_ANGLE_UNKNOWN : (int16_t)(2 * (int8_t)in[9]);
	r->ox_r = (float)in[10] / 100.0f;
	r->ox_conf = in[10] ? (float)(in[11] & 0x0F) / 15.0f : 0.0f;
}
