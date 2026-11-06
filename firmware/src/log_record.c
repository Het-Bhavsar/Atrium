/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "log_record.h"

#include <limits.h>
#include <math.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static void put16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static uint16_t get16(const uint8_t *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

/* VBAT in 10 mV steps above 2.50 V: 1..255 is 2.51 to 5.05 V, 0 = unknown */
static uint8_t vbat_code(uint16_t mv)
{
	if (mv <= 2500) {
		return 0;
	}
	uint32_t c = (mv - 2500U + 5U) / 10U;

	return c > 255 ? 255 : (uint8_t)c;
}

/* angle / step as an int8, -128 reserved for unknown */
static int8_t angle_code(int16_t deg, int step)
{
	if (deg == INT16_MIN) {
		return LOG_ANGLE_UNKNOWN;
	}
	int v = (deg >= 0 ? deg + step / 2 : deg - step / 2) / step;

	return (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v);
}

static int16_t angle_back(int8_t c, int step)
{
	return c == LOG_ANGLE_UNKNOWN ? INT16_MIN : (int16_t)(c * step);
}

void log_record_pack(const struct log_record *r, uint8_t out[LOG_RECORD_SIZE])
{
	put16(&out[0], (uint16_t)r->time);
	put16(&out[2], (uint16_t)(r->time >> 16));
	out[4] = r->bpm;
	out[5] = r->bpm_conf;
	put16(&out[6], (uint16_t)r->skin_cc);
	put16(&out[8], (uint16_t)r->top_cc);
	put16(&out[10], r->activity_mg);
	out[12] = vbat_code(r->vbat_mv);
	out[13] = (uint8_t)angle_code(r->pitch_deg, 1);
	out[14] = r->soc;
	out[15] = (uint8_t)((r->mode & 0x0F) | (r->docked ? 0x10 : 0) | (r->charging ? 0x20 : 0) |
			   (r->pulse_ir ? 0x40 : 0));
	put16(&out[16], r->rmssd_x10);
	out[18] = r->rr_diffs;
	out[19] = (uint8_t)angle_code(r->roll_deg, 2);
	out[20] = (uint8_t)((MIN(r->mlc_max, 7) & 0x07) | ((MIN(r->mlc_median, 7) & 0x07) << 3) |
			    ((r->mlc_class & 0x03) << 6));
	out[21] = r->steps_pm;
	uint8_t conf = r->ox_conf > 0.0f ? (uint8_t)MIN(lroundf(r->ox_conf * 15.0f), 15) : 0;
	long r100 = lroundf(r->ox_r * 100.0f);

	out[22] = (uint8_t)(MIN(r->pulse_s, 15) | (conf << 4));
	out[23] = (uint8_t)(r100 < 1 ? 0 : r100 > 255 ? 255 : r100);
}

void log_record_unpack(const uint8_t in[LOG_RECORD_SIZE], struct log_record *r)
{
	r->time = get16(&in[0]) | ((uint32_t)get16(&in[2]) << 16);
	r->bpm = in[4];
	r->bpm_conf = in[5];
	r->skin_cc = (int16_t)get16(&in[6]);
	r->top_cc = (int16_t)get16(&in[8]);
	r->activity_mg = get16(&in[10]);
	r->vbat_mv = in[12] ? (uint16_t)(2500 + 10 * in[12]) : 0;
	r->pitch_deg = angle_back((int8_t)in[13], 1);
	r->soc = in[14];
	r->mode = in[15] & 0x0F;
	r->docked = in[15] & 0x10;
	r->charging = in[15] & 0x20;
	r->pulse_ir = in[15] & 0x40;
	r->rmssd_x10 = get16(&in[16]);
	r->rr_diffs = in[18];
	r->roll_deg = angle_back((int8_t)in[19], 2);
	r->mlc_max = in[20] & 0x07;
	r->mlc_median = (in[20] >> 3) & 0x07;
	r->mlc_class = in[20] >> 6;
	r->steps_pm = in[21];
	r->pulse_s = in[22] & 0x0F;
	r->ox_conf = (float)(in[22] >> 4) / 15.0f;
	r->ox_r = (float)in[23] / 100.0f;
}
