/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Register encoding checked against the MAXM86161 data sheet (19-100523 Rev 1).
 */
#include <zephyr/ztest.h>

#include "maxm86161_regs.h"

ZTEST_SUITE(maxm86161, NULL, NULL, NULL, NULL, NULL);

ZTEST(maxm86161, test_led_codes_match_the_pa_table)
{
	uint8_t rge, code;

	/* 31 mA range: 0xFF = 31.0 mA, LSB 0.12 mA */
	zassert_ok(maxm86161_led_code(31000, &rge, &code));
	zassert_equal(rge, 0);
	zassert_equal(code, 0xFF);
	zassert_ok(maxm86161_led_code(15000, &rge, &code));
	zassert_equal(rge, 0);
	zassert_within(maxm86161_led_ua(rge, code), 15000, 61);
	/* the data sheet example: range 3 code 0x20 = 15.36 mA (table: 0.48 mA LSB) */
	zassert_within(maxm86161_led_ua(3, 0x20), 15560, 200);
	/* 40 mA needs the 62 mA range */
	zassert_ok(maxm86161_led_code(40000, &rge, &code));
	zassert_equal(rge, 1);
	zassert_equal(maxm86161_led_code(65000, &rge, &code), 0);
	zassert_equal(rge, 2);
	zassert_equal(maxm86161_led_code(125000, &rge, &code), -1);
}

ZTEST(maxm86161, test_sample_rate_codes)
{
	zassert_equal(maxm86161_sr_code(25), 0x00);
	zassert_equal(maxm86161_sr_code(50), 0x01);
	zassert_equal(maxm86161_sr_code(8), 0x0A);
	zassert_equal(maxm86161_sr_code(32), 0x0C);
	zassert_equal(maxm86161_sr_code(64), 0x0D);
	zassert_equal(maxm86161_sr_code(4), -1, "4 Hz is not a PPG_SR rate");
}

ZTEST(maxm86161, test_encode_sequence)
{
	struct maxm86161_slot s[3] = {{MAXM86161_LEDC_GREEN, 15000},
				      {MAXM86161_LEDC_IR, 10000},
				      {MAXM86161_LEDC_RED, 40000}};
	struct maxm86161_cfg_regs r;

	zassert_ok(maxm86161_encode(s, 3, 50, &r));
	zassert_equal(r.seq[0], 0x21); /* LEDC2 = IR (2), LEDC1 = green (1) */
	zassert_equal(r.seq[1], 0x03); /* LEDC4 = none, LEDC3 = red */
	zassert_equal(r.seq[2], 0x00);
	zassert_equal(r.range1, 0x10); /* LED3 (red) on the 62 mA range */
	zassert_equal(r.pa[0], 123);   /* 15 mA x 255 / 31 mA */
	zassert_equal(r.ppg_cfg2, 0x01 << 3);

	struct maxm86161_slot dup[2] = {{MAXM86161_LEDC_GREEN, 5000}, {MAXM86161_LEDC_GREEN, 9000}};

	zassert_equal(maxm86161_encode(dup, 2, 25, &r), -1, "one PA register per LED");
	zassert_equal(maxm86161_encode(s, 3, 30, &r), -1, "30 Hz is not a rate");
}

ZTEST(maxm86161, test_encode_direct_ambient)
{
	/* research modes: green, IR, red, then direct ambient (LEDC 0x9, data sheet Table 2) */
	struct maxm86161_slot s[4] = {{MAXM86161_LEDC_GREEN, 15000},
				      {MAXM86161_LEDC_IR, 10000},
				      {MAXM86161_LEDC_RED, 10000},
				      {MAXM86161_LEDC_DA, 0}};
	struct maxm86161_cfg_regs r;

	for (uint16_t hz = 100; hz <= 200; hz += 100) {
		zassert_ok(maxm86161_encode(s, 4, hz, &r), "%u Hz", hz);
		zassert_equal(r.seq[0], 0x21); /* LEDC2 = IR, LEDC1 = green */
		zassert_equal(r.seq[1], 0x93); /* LEDC4 = direct ambient, LEDC3 = red */
		zassert_equal(r.seq[2], 0x00);
		zassert_equal(r.range1, 0x00, "all three LEDs on the 31 mA range");
		zassert_equal(r.pa[0], 123);
		zassert_equal(r.pa[1], 82); /* 10 mA x 255 / 31 mA */
		zassert_equal(r.pa[2], 82);
	}
	zassert_equal(r.ppg_cfg2, 0x04 << 3, "PPG_SR 200 Hz");
	/* its FIFO tag is its slot: 0x04 */
	uint8_t w[3] = {0x04 << 3, 0x12, 0x34};
	uint8_t tag;
	uint32_t v;

	maxm86161_fifo_decode(w, &tag, &v);
	zassert_equal(tag, 0x04);
	zassert_equal(maxm86161_tag_slot(tag), 3);
	zassert_equal(v, 0x1234);

	struct maxm86161_slot two_da[2] = {{MAXM86161_LEDC_DA, 0}, {MAXM86161_LEDC_DA, 0}};

	zassert_equal(maxm86161_encode(two_da, 2, 100, &r), -1, "one ambient slot");
	struct maxm86161_slot bad[1] = {{0x8, 1000}}; /* pilot: not used */

	zassert_equal(maxm86161_encode(bad, 1, 100, &r), -1);
}

ZTEST(maxm86161, test_encode_sample_averaging)
{
	/* averaged research modes: PPG_SR 200 Hz (0x04), SMP_AVE 4 (0b010) or 8 (0b011) */
	struct maxm86161_slot s[4] = {{MAXM86161_LEDC_GREEN, 15000},
				      {MAXM86161_LEDC_IR, 10000},
				      {MAXM86161_LEDC_RED, 10000},
				      {MAXM86161_LEDC_DA, 0}};
	struct maxm86161_cfg_regs r;

	zassert_ok(maxm86161_encode_avg(s, 4, 200, 4, &r));
	zassert_equal(r.ppg_cfg2, (0x04 << 3) | 0x2);
	zassert_equal(r.seq[1], 0x93, "the slots are unchanged");
	zassert_ok(maxm86161_encode_avg(s, 4, 200, 8, &r));
	zassert_equal(r.ppg_cfg2, (0x04 << 3) | 0x3);
	zassert_ok(maxm86161_encode_avg(s, 4, 200, 1, &r));
	zassert_equal(r.ppg_cfg2, 0x04 << 3, "1 = no averaging");
	zassert_ok(maxm86161_encode_avg(s, 1, 25, 128, &r));
	zassert_equal(r.ppg_cfg2 & 0x7, 0x7);
	zassert_equal(maxm86161_encode_avg(s, 4, 200, 3, &r), -1, "not a power of 2");
	zassert_equal(maxm86161_encode_avg(s, 4, 200, 0, &r), -1);
	/* encode() is encode_avg() with 1 */
	zassert_ok(maxm86161_encode(s, 4, 200, &r));
	zassert_equal(r.ppg_cfg2, 0x04 << 3);
}

ZTEST(maxm86161, test_fifo_decode_and_tags)
{
	uint8_t tag;
	uint32_t v;
	const uint8_t a[3] = {0x0F, 0xFF, 0xFF}; /* tag 1, 0x7FFFF */
	const uint8_t b[3] = {0x10, 0x00, 0x2A}; /* tag 2, 42 */
	const uint8_t inv[3] = {0xF0, 0x00, 0x00};

	maxm86161_fifo_decode(a, &tag, &v);
	zassert_equal(tag, 1);
	zassert_equal(v, 0x7FFFF);
	zassert_equal(maxm86161_tag_slot(tag), 0);
	maxm86161_fifo_decode(b, &tag, &v);
	zassert_equal(tag, 2);
	zassert_equal(v, 42);
	zassert_equal(maxm86161_tag_slot(tag), 1);
	maxm86161_fifo_decode(inv, &tag, &v);
	zassert_equal(tag, MAXM86161_TAG_INVALID);
	zassert_equal(maxm86161_tag_slot(tag), -1);
	zassert_equal(maxm86161_tag_slot(0x0E), 1, "picket-fence replacement of LEDC2");
	zassert_equal(maxm86161_tag_slot(0x19), -1, "proximity data is not a slot");
}
