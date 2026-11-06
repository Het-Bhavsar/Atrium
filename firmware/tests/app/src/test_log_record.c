/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits.h>
#include <zephyr/ztest.h>

#include "log_record.h"

ZTEST_SUITE(log_record, NULL, NULL, NULL, NULL, NULL);

ZTEST(log_record, test_layout_matches_gatt_doc)
{
	struct log_record r = {.time = 0x11223344, .bpm = 72, .bpm_conf = 200, .skin_cc = -150,
			       .top_cc = 3321, .activity_mg = 0x0102, .vbat_mv = 3850, .soc = 87,
			       .mode = 5, .docked = true, .charging = false, .pulse_ir = true,
			       .rmssd_x10 = 423, .rr_diffs = 9, .pitch_deg = -12, .roll_deg = 91,
			       .mlc_max = 6, .mlc_median = 4, .mlc_class = 1, .steps_pm = 105,
			       .pulse_s = 14, .ox_conf = 0.8f, .ox_r = 0.4508f};
	uint8_t b[LOG_RECORD_SIZE];
	struct log_record back;

	log_record_pack(&r, b);
	zassert_equal(b[0], 0x44);
	zassert_equal(b[3], 0x11);
	zassert_equal(b[4], 72);
	zassert_equal(b[6], 0x6A); /* -150 = 0xFF6A */
	zassert_equal(b[7], 0xFF);
	zassert_equal(b[10], 0x02);
	zassert_equal(b[12], 135);         /* (3850 - 2500) / 10 */
	zassert_equal((int8_t)b[13], -12); /* pitch, 1 degree */
	zassert_equal(b[14], 87);
	zassert_equal(b[15], 0x55);
	zassert_equal(b[16] | (b[17] << 8), 423);
	zassert_equal(b[18], 9);
	zassert_equal((int8_t)b[19], 46);  /* roll, 2 degrees: 91 -> 46 */
	zassert_equal(b[20], 6 | (4 << 3) | (1 << 6), "MLC max, median, class walk");
	zassert_equal(b[21], 105);
	zassert_equal(b[22], 14 | (12 << 4), "pulse 14 s, burst confidence 0.8 x 15 = 12");
	zassert_equal(b[23], 45, "R x 100");

	log_record_unpack(b, &back);
	zassert_equal(back.time, r.time);
	zassert_equal(back.skin_cc, r.skin_cc);
	zassert_equal(back.top_cc, r.top_cc);
	zassert_equal(back.vbat_mv, 3850);
	zassert_equal(back.mode, 5);
	zassert_true(back.docked);
	zassert_false(back.charging);
	zassert_true(back.pulse_ir);
	zassert_equal(back.rmssd_x10, 423);
	zassert_equal(back.pitch_deg, -12);
	zassert_equal(back.roll_deg, 92);
	zassert_equal(back.mlc_max, 6);
	zassert_equal(back.mlc_median, 4);
	zassert_equal(back.mlc_class, 1);
	zassert_equal(back.steps_pm, 105);
	zassert_equal(back.pulse_s, 14);
	zassert_within((double)back.ox_conf, 0.8, 0.034);
	zassert_within((double)back.ox_r, 0.45, 0.001);
}

ZTEST(log_record, test_unknown_and_limits)
{
	struct log_record r = {.vbat_mv = 0, .pitch_deg = INT16_MIN, .roll_deg = INT16_MIN};
	uint8_t b[LOG_RECORD_SIZE];

	log_record_pack(&r, b);
	zassert_equal(b[12], 0, "unknown VBAT");
	zassert_equal((int8_t)b[13], LOG_ANGLE_UNKNOWN);
	zassert_equal((int8_t)b[19], LOG_ANGLE_UNKNOWN);
	zassert_equal(b[22], 0, "no pulse, no burst");
	zassert_equal(b[23], 0, "no burst R");

	r.vbat_mv = 6000;
	r.roll_deg = -180;
	r.pulse_s = 20;
	r.ox_conf = 0.01f; /* a burst that ended, with almost no confidence, still rounds to 0 */
	r.ox_r = 3.0f;
	log_record_pack(&r, b);
	zassert_equal(b[12], 255);
	zassert_equal((int8_t)b[19], -90);
	zassert_equal(b[22] & 0x0F, 15, "pulse seconds clamped");
	zassert_equal(b[23], 255, "R clamped at 2.55");
}
