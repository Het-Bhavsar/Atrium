/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Basic behaviour of the max-slope HRV port; agreement with the reference is checked in
 * test_ml_vectors.c.
 */
#include <math.h>
#include <zephyr/ztest.h>

#include "hrv.h"

static struct hrv h;

/* intensity counts of a pulse train with the given intervals */
static void feed(const float *rr_ms, int n_rr, int repeats)
{
	float beats[400];
	int nb = 0;
	float t = 1500.0f;

	for (int r = 0; r < repeats; r++) {
		for (int i = 0; i < n_rr && nb < 400; i++) {
			beats[nb++] = t;
			t += rr_ms[i];
		}
	}
	int samples = (int)(t / 1000.0f * HRV_FS);

	for (int k = 0; k < samples; k++) {
		float tk = k * 1000.0f / HRV_FS, v = 0;

		for (int b = 0; b < nb; b++) {
			float d = (tk - beats[b]) / 90.0f;

			if (d > -4 && d < 4) {
				v += expf(-0.5f * d * d);
			}
		}
		hrv_push(&h, (int32_t)(150000.0f - 900.0f * v));
	}
}

static struct hrv_window close_all(void)
{
	hrv_flush(&h);
	return hrv_close(&h, hrv_time_s(&h) - 0.5);
}

ZTEST_SUITE(hrv, NULL, NULL, NULL, NULL, NULL);

ZTEST(hrv, test_alternating_rr_gives_known_rmssd)
{
	/* 800 / 850 ms alternating: every successive difference is 50 ms */
	static const float rr[] = {800, 850};

	hrv_init(&h, 25);
	feed(rr, 2, 20);
	struct hrv_window w = close_all();

	zassert_true(w.n_diffs >= 30, "%u diffs", w.n_diffs);
	zassert_within(w.rmssd_x10, 500, 50, "rmssd %u", w.rmssd_x10);
}

ZTEST(hrv, test_steady_rhythm_is_near_zero)
{
	static const float rr[] = {1000};

	hrv_init(&h, 25);
	feed(rr, 1, 30);
	struct hrv_window w = close_all();

	zassert_true(w.n_diffs >= 20);
	zassert_true(w.rmssd_x10 < 50, "rmssd %u", w.rmssd_x10);
}

ZTEST(hrv, test_missed_beat_is_rejected)
{
	static const float rr[] = {800, 850, 800, 850, 800, 850, 1650, 800, 850, 800, 850, 800};

	hrv_init(&h, 25);
	feed(rr, ARRAY_SIZE(rr), 3);
	struct hrv_window w = close_all();

	zassert_within(w.rmssd_x10, 500, 60, "rmssd %u", w.rmssd_x10);
}

ZTEST(hrv, test_too_few_beats_reports_zero)
{
	static const float rr[] = {900};

	hrv_init(&h, 25);
	feed(rr, 1, 5);
	struct hrv_window w = close_all();

	zassert_equal(w.rmssd_x10, 0);
	zassert_true(w.n_diffs < HRV_MIN_DIFFS);
}

ZTEST(hrv, test_only_at_25_hz)
{
	static const float rr[] = {900};

	hrv_init(&h, 32);
	feed(rr, 1, 20);
	zassert_equal(close_all().beats, 0);
}
