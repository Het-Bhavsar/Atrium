/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pins the byte layouts of docs/ble_gatt.md, which the app builds against.
 */
#include <math.h>
#include <zephyr/ztest.h>

#include "gatt_encode.h"
#include "log_record.h"

ZTEST_SUITE(gatt_encode, NULL, NULL, NULL, NULL, NULL);

ZTEST(gatt_encode, test_charge_state)
{
	struct app_status s = {
		.power = {.state = POD_DOCKED, .chg = CHG_COLD_LIMITED, .batt = BATT_LOW,
			  .band = CHG_BAND_COLD, .docked = true, .chg_disable = true,
			  .chg_cold = true, .mode = MODE_DOCKED_IDLE},
		.soc = 42, .vsys_mv = 4500,
		.skin_valid = true, .skin_cc = 3300, .top_valid = true, .top_cc = 790,
		.chg_stat0 = 0x01, .chg_stat1 = 0x04, .on_skin = false,
	};
	uint8_t b[GATT_CHARGE_STATE_LEN];

	zassert_equal(gatt_encode_charge_state(&s, b), 19);
	zassert_equal(b[0], 1);
	zassert_equal(b[1], 3);
	zassert_equal(b[2], 1);
	zassert_equal(b[3], 2);
	zassert_equal(b[4], 0x07);
	zassert_equal(b[5], 42);
	zassert_equal(b[6] | (b[7] << 8), 4500, "VSYS");
	zassert_equal(b[8] | (b[9] << 8), 0, "reserved: no VBUS sense");
	zassert_equal((int16_t)(b[10] | (b[11] << 8)), 790); /* TH4 top, primary */
	zassert_equal((int16_t)(b[12] | (b[13] << 8)), INT16_MIN, "reserved: no backup");
	zassert_equal(b[14], 0x01, "BQ21061 STAT0: VIN good");
	zassert_equal(b[15], 0x04, "BQ21061 STAT1: TS cool");
	zassert_equal(b[16], MODE_DOCKED_IDLE);
	zassert_equal((int16_t)(b[17] | (b[18] << 8)), INT16_MIN);

	s.on_skin = true;
	gatt_encode_charge_state(&s, b);
	zassert_equal(b[4], 0x0F);

	s.on_skin = false;
	s.on_case = true;
	gatt_encode_charge_state(&s, b);
	zassert_equal(b[4], 0x17, "bit 4: fed by the charging case");

	s.power.docked = false;
	gatt_encode_charge_state(&s, b);
	zassert_equal(b[14] | b[15], 0, "no charger status off the Plinth");
}

ZTEST(gatt_encode, test_skin_temp_and_activity)
{
	struct app_status s = {.skin_valid = true, .skin_cc = 3412, .top_cc = -40,
			       .activity_mg = 123, .moving = true, .mlc_max = 5, .mlc_class = 1,
			       .steps_pm = 104};
	uint8_t t[GATT_SKIN_TEMP_LEN], a[GATT_ACTIVITY_LEN];

	zassert_equal(gatt_encode_skin_temp(&s, 0x01020304, t), 9);
	zassert_equal(t[0] | (t[1] << 8), 3412);
	zassert_equal((int16_t)(t[2] | (t[3] << 8)), -40);
	zassert_equal(t[4], 0x01);
	zassert_equal(t[5], 0x04);
	zassert_equal(t[8], 0x01);

	zassert_equal(gatt_encode_activity(&s, 7, a), 10);
	zassert_equal(a[0], 7);
	zassert_equal(a[4] | (a[5] << 8), 123);
	zassert_equal(a[6], 0x01);
	zassert_equal(a[7], 5, "max MLC level");
	zassert_equal(a[8], 1, "class walk");
	zassert_equal(a[9], 104, "steps/min");
}

ZTEST(gatt_encode, test_oxygen_ratio)
{
	struct app_status s = {.ox_time = 0x01020304, .ox_r = 0.4508f, .ox_conf = 0.97f,
			       .ox_good = 12};
	uint8_t b[GATT_OXYGEN_LEN];

	zassert_equal(gatt_encode_oxygen(&s, b), 8);
	zassert_equal(b[0], 0x04);
	zassert_equal(b[3], 0x01);
	zassert_equal(b[4] | (b[5] << 8), 451, "R x 1000");
	zassert_equal(b[6], 247, "0.97 x 255");
	zassert_equal(b[7], 12);

	s.ox_r = NAN;
	s.ox_conf = 0.0f;
	s.ox_good = 2;
	gatt_encode_oxygen(&s, b);
	zassert_equal(b[4] | (b[5] << 8), 0, "no R");
	zassert_equal(b[6], 0);
}

ZTEST(gatt_encode, test_log_info_and_sleep_window)
{
	uint8_t b[GATT_LOG_INFO_LEN];
	struct app_settings s = APP_SETTINGS_DEFAULT;
	uint8_t w[GATT_SLEEP_WINDOW_LEN];

	/* 32 KB log: pages 12..19 held, 8 pages */
	zassert_equal(gatt_encode_log_info(12, 19, 8, b), 18);
	zassert_equal(b[0], 12);
	zassert_equal(b[4] | (b[5] << 8), 19);
	zassert_equal(b[8] | (b[9] << 8), 4096, "page size");
	zassert_equal(b[10], 30, "epoch, s");
	zassert_equal(b[12] | (b[13] << 8) | (b[14] << 16), 8);
	zassert_equal(b[16], 1, "format");
	zassert_equal(b[17], 12, "record size");

	gatt_encode_sleep_window(&s, w);
	zassert_equal(w[0] | (w[1] << 8), 23 * 60);
	zassert_equal(w[2] | (w[3] << 8), 7 * 60);

	const uint8_t ok[6] = {0x50, 0x05, 0x68, 0x01, 0x4A, 0x01}; /* 22:40, 06:00, +5:30 */
	const uint8_t bad[6] = {0xA0, 0x05, 0, 0, 0, 0};             /* 1440: out of range */

	zassert_ok(gatt_decode_sleep_window(ok, 6, &s));
	zassert_equal(s.sleep_start, 1360);
	zassert_equal(s.sleep_end, 360);
	zassert_equal(s.utc_offset_min, 330);
	zassert_equal(gatt_decode_sleep_window(bad, 6, &s), -1);
	zassert_equal(gatt_decode_sleep_window(ok, 5, &s), -1);
}

/* 7 Live Epoch 0x0504: time, head page, then the record exactly as the log holds it */
ZTEST(gatt_encode, test_live_epoch)
{
	struct log_record r = {.bpm = 63.4f, .conf = 0.8f, .on_skin = true, .rmssd_ms = 41.0f,
			       .n_diffs = 22, .activity_mg = 7.0f, .pulse_s = 30, .skin_cc = 3412,
			       .steps = 0, .pitch_deg = -88, .roll_deg = 10};
	uint8_t b[GATT_LIVE_EPOCH_LEN], rec[LOG_RECORD_SIZE];

	zassert_equal(gatt_encode_live_epoch(0x80000123U, 9, &r, b), 20, "one 23-byte MTU packet");
	zassert_equal(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24, 0x80000123U,
		      "bit 31: uptime, as in the log");
	zassert_equal(b[4] | b[5] << 8 | b[6] << 16 | (uint32_t)b[7] << 24, 9, "head page");
	log_record_pack(&r, rec);
	zassert_mem_equal(&b[8], rec, LOG_RECORD_SIZE, "the logged bytes");
	zassert_equal(b[8], 63);
	zassert_true(log_record_written(&b[8]));
}

ZTEST(gatt_encode, test_motion_rate_field)
{
	/* mHz as before up to 32.767 Hz; above it bit 15 and 10 mHz steps */
	zassert_equal(gatt_motion_rate(12500), 12500);
	zassert_equal(gatt_motion_rate(0), 0);
	zassert_equal(gatt_motion_rate(32767), 32767);
	zassert_equal(gatt_motion_rate(104000), 0xA8A0, "104 Hz in the research modes");
	zassert_equal(gatt_motion_rate(104000) & 0x7FFF, 10400);
	zassert_equal(gatt_motion_rate(10000000), 0xFFFF, "saturates");
}
