/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "epoch.h"

ZTEST_SUITE(epoch, NULL, NULL, NULL, NULL, NULL);

static const struct epoch_close_in quiet = {
	.hrv = {.rmssd_x10 = 423, .n_diffs = 21},
	.mlc_max = 2,
	.cls = 1,
	.steps_pm = 104,
	.pitch_deg = -12,
	.roll_deg = 40,
	.activity_mg = 33,
	.skin_valid = true,
	.skin_cc = 3450,
};

ZTEST(epoch, test_worn_epoch)
{
	struct epoch e;
	struct log_record r;

	epoch_open(&e, 1000);
	for (int s = 0; s < 30; s++) {
		struct epoch_second es = {.on_skin = true, .pulse_present = s >= 4,
					  .bpm = s < 10 ? 0 : (s & 1 ? 61 : 63), .conf = 0.6f};

		epoch_second(&e, &es);
	}
	zassert_true(epoch_close(&e, &quiet, &r));
	zassert_within(r.bpm, 62.0f, 0.01f, "mean of the 20 reported seconds");
	zassert_within(r.conf, 0.6f, 0.001f);
	zassert_true(r.on_skin);
	zassert_false(r.docked || r.charging || r.pulse_ir);
	zassert_equal(r.pulse_s, 26);
	zassert_within(r.rmssd_ms, 42.3f, 0.01f);
	zassert_equal(r.n_diffs, 21);
	zassert_equal(r.steps, 52, "104 steps/min over 30 s, walking");
	zassert_equal(r.skin_cc, 3450);
	zassert_equal(r.ox_r, 0.0f);
}

ZTEST(epoch, test_still_epoch_has_no_steps_and_burst_lands)
{
	struct epoch e;
	struct log_record r;
	struct epoch_close_in in = quiet;

	in.cls = 0;
	in.ox = true;
	in.ox_r = 0.52f;
	in.ox_conf = 0.7f;
	in.skin_valid = false;
	epoch_open(&e, 0);
	for (int s = 0; s < 30; s++) {
		struct epoch_second es = {.on_skin = s < 10, .pulse_ir = true};

		epoch_second(&e, &es);
	}
	zassert_true(epoch_close(&e, &in, &r));
	zassert_equal(r.steps, 0);
	zassert_false(r.on_skin, "worn 10 of 30 s");
	zassert_true(r.pulse_ir);
	zassert_equal(r.ox_r, 0.52f);
	zassert_equal(r.skin_cc, LOG_SKIN_UNKNOWN);
}

ZTEST(epoch, test_docked_epoch_not_logged)
{
	struct epoch e;
	struct log_record r;

	epoch_open(&e, 0);
	for (int s = 0; s < 30; s++) {
		struct epoch_second es = {.docked = true, .charging = true};

		epoch_second(&e, &es);
	}
	zassert_false(epoch_close(&e, &quiet, &r), "wholly on the dock: a gap in the log");
	/* the epoch the pod was lifted in is logged, with the docked flag */
	epoch_open(&e, 30);
	for (int s = 0; s < 30; s++) {
		struct epoch_second es = {.docked = s < 5, .charging = s < 5};

		epoch_second(&e, &es);
	}
	zassert_true(epoch_close(&e, &quiet, &r));
	zassert_true(r.docked && r.charging);
}

ZTEST(epoch, test_next_start_time)
{
	zassert_equal(epoch_next_t0(0, false, 1234), 1234, "first epoch: the clock");
	zassert_equal(epoch_next_t0(1000, true, 1031), 1030, "runs on in 30 s steps");
	zassert_equal(epoch_next_t0(1000, true, 1028), 1030);
	zassert_equal(epoch_next_t0(1000, true, 1040), 1040, "a jump: the clock");
	/* the phone set the clock: uptime time base to Unix time */
	uint32_t up = LOG_TIME_UPTIME | 1000;

	zassert_equal(epoch_next_t0(up, true, up + 30), up + 30);
	zassert_equal(epoch_next_t0(up, true, 1800000000), 1800000000);
}
