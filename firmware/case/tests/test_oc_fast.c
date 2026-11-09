/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The fast over-current cut-off: trips on the second reading in a row over the limit,
 * which at one reading per 20 ms frame is within the 50 ms the feed diodes allow.
 */
#include "case_sm.h"
#include "check.h"
#include "oc_fast.h"

#define FRAME_MS 20

static void trips_on_second_reading(void)
{
	struct oc_fast f = {0};

	CHECK(!oc_fast_sample(&f, true, 15000, false, 80000));
	CHECK(!oc_fast_sample(&f, true, 300000, false, 80000));
	CHECK(oc_fast_sample(&f, true, 300000, false, 80000));
}

static void one_spike_does_not_trip(void)
{
	struct oc_fast f = {0};

	CHECK(!oc_fast_sample(&f, true, 300000, false, 80000));
	CHECK(!oc_fast_sample(&f, true, 15000, false, 80000));
	CHECK(!oc_fast_sample(&f, true, 300000, false, 80000));
}

static void saturation_counts_as_over(void)
{
	struct oc_fast f = {0};

	CHECK(!oc_fast_sample(&f, true, 0, true, 80000));
	CHECK(oc_fast_sample(&f, true, 0, true, 80000));
}

static void disarmed_never_trips_and_resets(void)
{
	struct oc_fast f = {0};

	CHECK(!oc_fast_sample(&f, true, 300000, false, 80000));
	CHECK(!oc_fast_sample(&f, false, 300000, false, 80000)); /* blanking or a pulse */
	CHECK(!oc_fast_sample(&f, true, 300000, false, 80000));  /* counts from one again */
	CHECK(oc_fast_sample(&f, true, 300000, false, 80000));
}

/* a short that starts just after a reading is caught within OC_FAST_COUNT frames */
static void worst_case_within_50_ms(void)
{
	struct oc_fast f = {0};
	int t = 0, cut = -1;

	oc_fast_sample(&f, true, 15000, false, 80000); /* reading at t = 0, just before the short */
	for (t = FRAME_MS; t <= 200; t += FRAME_MS) {
		if (oc_fast_sample(&f, true, 300000, false, 80000)) {
			cut = t;
			break;
		}
	}
	CHECK(cut > 0);
	CHECK(cut <= 50); /* short at t = 0+, cut at 40 ms */
}

/* the state machine takes a fast trip as a contact fault and retries after 30 s */
static void fast_trip_is_a_fault(void)
{
	static const struct case_cfg cfg = CASE_CFG_DEFAULT;
	struct case_sm sm;
	struct case_in in = {0};
	struct case_out out = {0};
	uint32_t now = 1000;

	case_sm_init(&sm, &cfg, now);
	in.vbat_mv = 3900;
	in.temp_c10 = CASE_TEMP_NONE;
	in.lid_open = true;
	for (int i = 0; i < 5; i++, now += CASE_TICK_MS) {
		in.now_ms = now;
		in.i_valid = out.vin_en && out.opa_on;
		in.i_ua = in.i_valid ? 15000 : 0;
		case_sm_step(&sm, &in, &out);
	}
	CHECK(out.vin_en);
	CHECK(out.oc_armed);
	in.now_ms = now;
	in.fast_trip = true;
	case_sm_step(&sm, &in, &out);
	CHECK(!out.vin_en);
	CHECK(!out.oc_armed);
	CHECK(sm.fault);
	CHECK_EQ(out.led, LED_FAULT);
	in.fast_trip = false;
	for (uint32_t t = 0; t < 31000; t += CASE_TICK_MS) {
		now += CASE_TICK_MS;
		in.now_ms = now;
		in.i_valid = out.vin_en && out.opa_on;
		in.i_ua = in.i_valid ? 15000 : 0;
		case_sm_step(&sm, &in, &out);
	}
	CHECK(!sm.fault);
	CHECK(out.vin_en);
}

const struct check_case fast_cases[] = {
	{"fast: trips on the second reading over", trips_on_second_reading},
	{"fast: one spike does not trip", one_spike_does_not_trip},
	{"fast: a clipped reading counts as over", saturation_counts_as_over},
	{"fast: disarmed never trips", disarmed_never_trips_and_resets},
	{"fast: cut within 50 ms of a short", worst_case_within_50_ms},
	{"fast: the state machine treats it as a fault", fast_trip_is_a_fault},
	{0, 0},
};
