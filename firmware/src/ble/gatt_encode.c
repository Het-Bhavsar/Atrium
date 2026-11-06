/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gatt_encode.h"

#include <math.h>

#include "log_record.h"

static void le16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static void le32(uint8_t *p, uint32_t v)
{
	le16(p, (uint16_t)v);
	le16(p + 2, (uint16_t)(v >> 16));
}

#define CLAMP_F(v) ((v) > 1.0f ? 1.0f : (v) < 0.0f ? 0.0f : (v))
static int16_t clamp16(int32_t v)
{
	return v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : (int16_t)v;
}

size_t gatt_encode_charge_state(const struct app_status *s, uint8_t out[GATT_CHARGE_STATE_LEN])
{
	out[0] = (uint8_t)s->power.state;
	out[1] = (uint8_t)s->power.chg;
	out[2] = (uint8_t)s->power.batt;
	out[3] = (uint8_t)s->power.band;
	out[4] = (uint8_t)((s->power.docked ? 0x01 : 0) | (s->power.chg_inhibit ? 0x02 : 0) |
			   (s->chrg_n_low ? 0x04 : 0) | (s->on_skin ? 0x08 : 0));
	out[5] = s->soc;
	le16(&out[6], s->vbat_mv);
	le16(&out[8], s->power.docked ? s->vbus_mv : 0);
	le16(&out[10], (uint16_t)(s->top_valid ? s->top_cc : INT16_MIN));   /* TH4, primary */
	/* reserved since r5 (no TH3, no fuel gauge): fixed "no reading" values */
	le16(&out[12], (uint16_t)INT16_MIN);
	out[14] = 0;
	out[15] = (uint8_t)INT8_MIN;
	out[16] = (uint8_t)s->power.mode;
	le16(&out[17], (uint16_t)INT16_MIN);
	return GATT_CHARGE_STATE_LEN;
}

size_t gatt_encode_skin_temp(const struct app_status *s, uint32_t time,
			     uint8_t out[GATT_SKIN_TEMP_LEN])
{
	le16(&out[0], (uint16_t)clamp16(s->skin_cc));
	le16(&out[2], (uint16_t)clamp16(s->top_cc));
	out[4] = (uint8_t)((s->skin_valid ? 0x01 : 0) | (s->top_valid ? 0x02 : 0));
	le32(&out[5], time);
	return GATT_SKIN_TEMP_LEN;
}

size_t gatt_encode_activity(const struct app_status *s, uint32_t time,
			    uint8_t out[GATT_ACTIVITY_LEN])
{
	le32(&out[0], time);
	le16(&out[4], s->activity_mg);
	out[6] = (uint8_t)((s->moving ? 0x01 : 0) | (s->still ? 0x02 : 0));
	out[7] = s->mlc_max;
	out[8] = s->mlc_class;
	out[9] = s->steps_pm;
	return GATT_ACTIVITY_LEN;
}

size_t gatt_encode_oxygen(const struct app_status *s, uint8_t out[GATT_OXYGEN_LEN])
{
	long r = isnan(s->ox_r) ? 0 : lroundf(s->ox_r * 1000.0f);

	le32(&out[0], s->ox_time);
	le16(&out[4], (uint16_t)(r < 0 ? 0 : r > UINT16_MAX ? UINT16_MAX : r));
	out[6] = (uint8_t)lroundf(CLAMP_F(s->ox_conf) * 255.0f);
	out[7] = s->ox_good;
	return GATT_OXYGEN_LEN;
}

size_t gatt_encode_log_info(uint32_t oldest, uint32_t next, uint16_t interval_s, uint32_t capacity,
			    uint8_t out[GATT_LOG_INFO_LEN])
{
	le32(&out[0], oldest);
	le32(&out[4], next);
	le16(&out[8], LOG_RECORD_SIZE);
	le16(&out[10], interval_s);
	le32(&out[12], capacity);
	return GATT_LOG_INFO_LEN;
}

size_t gatt_encode_sleep_window(const struct app_settings *s, uint8_t out[GATT_SLEEP_WINDOW_LEN])
{
	le16(&out[0], s->sleep_start);
	le16(&out[2], s->sleep_end);
	le16(&out[4], (uint16_t)s->utc_offset_min);
	return GATT_SLEEP_WINDOW_LEN;
}

int gatt_decode_sleep_window(const uint8_t *in, size_t len, struct app_settings *s)
{
	if (len != GATT_SLEEP_WINDOW_LEN) {
		return -1;
	}
	uint16_t start = (uint16_t)(in[0] | (in[1] << 8));
	uint16_t end = (uint16_t)(in[2] | (in[3] << 8));
	int16_t off = (int16_t)(in[4] | (in[5] << 8));

	if (start >= 1440 || end >= 1440 || off < -14 * 60 || off > 14 * 60) {
		return -1;
	}
	s->sleep_start = start;
	s->sleep_end = end;
	s->utc_offset_min = off;
	return 0;
}
