/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Basic behaviour of the spectral pulse estimate; accuracy is checked against the reference
 * vectors in test_ml_vectors.c.
 */
#include <math.h>
#include <zephyr/ztest.h>

#include "pulse.h"

static struct pulse_est p;

/* PPG intensity counts: DC minus a pulse with a second harmonic (intensity falls with volume) */
static int feed(float bpm, float secs)
{
	int outs = 0;
	const int16_t still[3] = {0, 0, 1000};

	for (int i = 0; i < (int)(secs * PULSE_FS); i++) {
		float t = (float)i / PULSE_FS, f = bpm / 60.0f;
		float v = sinf(2 * 3.14159265f * f * t) + 0.3f * sinf(4 * 3.14159265f * f * t);

		outs += pulse_push(&p, (int32_t)(200000.0f - 1500.0f * v), still);
	}
	return outs;
}

ZTEST_SUITE(pulse, NULL, NULL, NULL, NULL, NULL);

ZTEST(pulse, test_one_output_a_second_after_8_s)
{
	struct pulse_out o;

	pulse_init(&p, 25);
	zassert_equal(feed(72, 7.96f), 0);
	zassert_false(pulse_get(&p, &o));
	zassert_equal(feed(72, 12), 12);
	zassert_true(pulse_get(&p, &o));
}

ZTEST(pulse, test_rates)
{
	static const float bpms[] = {45, 72, 110, 150, 185};
	struct pulse_out o;

	for (size_t b = 0; b < ARRAY_SIZE(bpms); b++) {
		pulse_init(&p, 25);
		feed(bpms[b], 20);
		pulse_get(&p, &o);
		zassert_within(o.bpm, bpms[b], 1.0f, "%.1f for %.0f", (double)o.bpm, (double)bpms[b]);
		zassert_true(o.valid);
		zassert_true(o.pulse_present, "a clean pulse is present");
		zassert_true(isnan(o.acc_peak_bpm), "still: no accelerometer peak");
	}
}

ZTEST(pulse, test_only_at_25_hz)
{
	pulse_init(&p, 8); /* the off-body probe */
	zassert_equal(feed(72, 20), 0);
}
