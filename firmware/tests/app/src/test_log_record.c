/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * The 12-byte record against app/python/atrium_ml/logrec.py encode(), byte for byte.
 */
#include <stdlib.h>
#include <zephyr/ztest.h>

#include "log_record.h"
#include "log_vec.h"

ZTEST_SUITE(log_record, NULL, NULL, NULL, NULL, NULL);

static void hex(const char *s, uint8_t out[LOG_RECORD_SIZE])
{
	for (int i = 0; i < LOG_RECORD_SIZE; i++) {
		char b[3] = {s[2 * i], s[2 * i + 1], 0};

		out[i] = (uint8_t)strtoul(b, NULL, 16);
	}
}

/* python3 -c "from atrium_ml import logrec" ... encode(ep(k)).hex(), the generator of log_vec.h */
ZTEST(log_record, test_matches_logrec_encode)
{
	static const struct {
		int k;
		const char *hex;
	} v[] = {
		{0, "0000000000000000a6a60000"},    {1, "3311014107090001a6a60000"},
		{7, "007707c7313f0007a6a60000"},    {123, "35bb7bf5ffc6717b17da0000"},
		{255, "41fffeedffc3f5ffd31c050f"},  {299, "6dbbfed3ff12feffff320000"},
		{1000, "5a88641464515a64a6000000"},
	};

	for (size_t i = 0; i < ARRAY_SIZE(v); i++) {
		struct log_record r = vec_epoch(v[i].k);
		uint8_t want[LOG_RECORD_SIZE], got[LOG_RECORD_SIZE];

		hex(v[i].hex, want);
		log_record_pack(&r, got);
		zassert_mem_equal(got, want, LOG_RECORD_SIZE, "epoch %d", v[i].k);
		zassert_true(log_record_written(got));
	}
}

ZTEST(log_record, test_round_trip)
{
	struct log_record r = {.bpm = 61.4f, .conf = 0.8f, .on_skin = true, .pulse_ir = true,
			       .rmssd_ms = 42.3f, .n_diffs = 21, .cls = 1, .activity_mg = 37.0f,
			       .mlc_max = 3, .pulse_s = 28, .skin_cc = 3462, .steps = 54,
			       .pitch_deg = -12, .roll_deg = 91, .ox_r = 0.45f, .ox_conf = 0.6f};
	struct log_record o;
	uint8_t b[LOG_RECORD_SIZE];

	log_record_pack(&r, b);
	log_record_unpack(b, &o);
	zassert_equal(o.bpm, 61.0f);
	zassert_within(o.conf, 0.8f, 0.04f);
	zassert_true(o.on_skin && o.pulse_ir && !o.docked && !o.charging);
	zassert_equal(o.rmssd_ms, 42.0f);
	zassert_equal(o.n_diffs, 21);
	zassert_equal(o.cls, 1);
	zassert_equal(o.mlc_max, 3);
	zassert_equal(o.pulse_s, 28);
	zassert_equal(o.skin_cc, 3460, "0.1 C steps");
	zassert_equal(o.steps, 54);
	zassert_equal(o.pitch_deg, -12);
	zassert_equal(o.roll_deg, 92, "2 degree steps, half to even");
	zassert_within(o.ox_r, 0.45f, 0.001f);
	zassert_within(o.ox_conf, 0.6f, 0.04f);
}

ZTEST(log_record, test_unknown_and_limits)
{
	struct log_record r = {.rmssd_ms = 0.3f, .skin_cc = LOG_SKIN_UNKNOWN,
			       .pitch_deg = LOG_ANGLE_UNKNOWN, .roll_deg = LOG_ANGLE_UNKNOWN,
			       .steps = 900, .pulse_s = 31, .n_diffs = 99, .activity_mg = 999.0f,
			       .ox_r = 0.0f, .ox_conf = 1.0f};
	uint8_t b[LOG_RECORD_SIZE];

	log_record_pack(&r, b);
	zassert_equal(b[2], 1, "RMSSD under 0.5 ms is still a value");
	zassert_equal(b[3], 63);
	zassert_equal(b[4], 255);
	zassert_equal(b[5] >> 3, 30);
	zassert_equal(b[6], 0);
	zassert_equal(b[7], 255);
	zassert_equal((int8_t)b[8], -128);
	zassert_equal((int8_t)b[9], -128);
	zassert_equal(b[10], 0);
	zassert_equal(b[11], 0, "no burst, no confidence");

	r.skin_cc = 2504; /* 25.04 C: below the range */
	log_record_pack(&r, b);
	zassert_equal(b[6], 0);
	r.skin_cc = 5200;
	log_record_pack(&r, b);
	zassert_equal(b[6], 254);

	static const uint8_t erased[LOG_RECORD_SIZE] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	zassert_false(log_record_written(erased));
}
