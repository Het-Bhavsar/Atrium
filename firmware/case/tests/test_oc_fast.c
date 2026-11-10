/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The fast over-current cut-off: trips on the second reading in a row over the limit,
 * which at one reading per 20 ms frame is within the 50 ms the feed diodes allow. The
 * last tests run the state machine and main.c's tick and frame timing together (sim_*),
 * with a pod model, and time the cut from the VIN edge.
 */
#include <string.h>

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
	in.now_ms = now;
	in.fast_trip = true;
	case_sm_step(&sm, &in, &out);
	CHECK(!out.vin_en);
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


/* ---- main.c's loop on the host: 100 ms ticks, five 20 ms frames each ---- */

#define ZERO_MS 10 /* main's settle delay for the zero reading, before VIN goes on */

enum pod_case { POD_INRUSH, POD_SHORT_FROM_START, POD_SHORT_IN_PULSE, POD_SHORT_MID_PULSE };

struct sim {
	struct case_cfg cfg;
	struct case_sm sm;
	struct case_in in;
	struct case_out out;
	struct oc_fast fast;
	enum pod_case pod;
	uint32_t t;        /* the tick time */
	bool vin;          /* the VIN_EN pin */
	uint32_t edge_t;   /* its last rising edge */
	int edges;
	bool shorted;
	uint32_t short_t;  /* when the short started */
	bool cut;
	uint32_t cut_t;
	bool fast_trip;    /* for the next tick */
	int trips;
};

static struct sim S;

/* what the shunt reads at `t` */
static int32_t sim_pod_ua(uint32_t t, bool *sat)
{
	*sat = false;
	if (!S.vin) {
		return 0;
	}
	if (S.shorted) {
		*sat = true; /* the SY6280 limit, 0.34-0.57 A, clips the 1 ohm x 21 reading */
		return 450000;
	}
	if (S.pod == POD_INRUSH && t - S.edge_t < 9) {
		return 300000; /* the pod's input capacitors and charger start-up */
	}
	return 15000; /* charging */
}

static void sim_vin(bool on, uint32_t t)
{
	if (on && !S.vin) {
		S.edge_t = t;
		S.edges++;
		S.vin = true;
		if (S.pod == POD_SHORT_FROM_START && S.edges == 1) {
			S.shorted = true;
			S.short_t = t;
		}
		if (S.pod == POD_SHORT_IN_PULSE && S.sm.pulse && !S.shorted) {
			S.shorted = true; /* shorted as VIN comes back in the signature pattern */
			S.short_t = t;
		}
	} else if (!on) {
		S.vin = false;
	}
	oc_fast_vin(&S.fast, on, t);
}

/* one frame of main.c's loop */
static void sim_frame(uint32_t t)
{
	if (oc_fast_armed(&S.fast, t, S.cfg.oc_blank_ms)) {
		bool sat;
		int32_t ua = sim_pod_ua(t, &sat);

		if (oc_fast_sample(&S.fast, true, ua, sat, S.cfg.oc_ua)) {
			sim_vin(false, t);
			S.fast_trip = true;
			S.trips++;
			if (!S.cut) {
				S.cut = true;
				S.cut_t = t;
			}
		}
	}
	/* just after this frame's reading: the worst phase */
	if (S.pod == POD_SHORT_MID_PULSE && S.sm.pulse && S.vin && !S.shorted &&
	    t - S.edge_t >= 100 && S.edges >= 2) {
		S.shorted = true; /* 100 ms into an energised pulse interval */
		S.short_t = t;
	}
}

static void sim_tick(void)
{
	uint32_t T = S.t, w = T;

	S.in.now_ms = T;
	S.in.fast_trip = S.fast_trip;
	S.fast_trip = false;
	S.in.i_valid = S.out.vin_en && S.out.opa_on;
	S.in.i_ua = S.in.i_valid ? sim_pod_ua(T, &S.in.i_sat) : 0;
	if (!S.in.i_valid) {
		S.in.i_sat = false;
	}
	S.in.pod_pullup = S.out.ldo_pwr && S.vin;
	case_sm_step(&S.sm, &S.in, &S.out);
	if (S.out.zero_req) {
		w += ZERO_MS;
	}
	sim_vin(S.out.vin_en, w);
	for (int f = 0; f < CASE_TICK_MS / FRAME_MS; f++) {
		uint32_t ft = T + (uint32_t)f * FRAME_MS;

		sim_frame(ft > w ? ft : w);
	}
	S.t = T + CASE_TICK_MS;
}

static void sim_init(enum pod_case pod)
{
	static const struct case_cfg cfg = CASE_CFG_DEFAULT;

	memset(&S, 0, sizeof(S));
	S.cfg = cfg;
	S.pod = pod;
	S.t = 1000;
	case_sm_init(&S.sm, &S.cfg, S.t);
	S.in.vbat_mv = 3900;
	S.in.temp_c10 = CASE_TEMP_NONE;
	S.in.lid_open = true; /* lid open: VIN on, pod docked */
}

static void sim_run(uint32_t ms)
{
	for (uint32_t e = 0; e < ms; e += CASE_TICK_MS) {
		sim_tick();
	}
}

/* (a) a pod shorted from the start: cut within the blanking plus two frames (and one spare) */
static void short_from_feed_start(void)
{
	sim_init(POD_SHORT_FROM_START);
	sim_run(1000);
	CHECK(S.shorted);
	CHECK(S.cut);
	CHECK(S.cut_t - S.short_t <= (uint32_t)S.cfg.oc_blank_ms + 3 * FRAME_MS);
	CHECK(S.cut_t - S.short_t <= 50); /* B-8: the feed diodes */
	CHECK(S.sm.fault);
	CHECK(!S.out.vin_en);
	printf("     startup short cut %u ms after VIN on\n", (unsigned)(S.cut_t - S.short_t));
}

/* (b) a short as VIN comes back inside the case signature, and one 100 ms into an
 * energised pulse interval: both cut while the pattern runs */
static void short_in_signature_pulse(void)
{
	sim_init(POD_SHORT_IN_PULSE);
	sim_run(6000);
	CHECK(S.shorted);
	CHECK(S.cut);
	CHECK(S.cut_t - S.short_t <= (uint32_t)S.cfg.oc_blank_ms + 3 * FRAME_MS);
	CHECK(S.sm.fault);
	CHECK(!S.out.vin_en);
	printf("     short at a pulse-on edge cut %u ms after it\n", (unsigned)(S.cut_t - S.short_t));

	sim_init(POD_SHORT_MID_PULSE);
	sim_run(6000);
	CHECK(S.shorted);
	CHECK(S.cut);
	CHECK(S.cut_t - S.short_t <= 2 * FRAME_MS);
	CHECK(S.sm.fault);
	printf("     short inside a pulse interval cut %u ms after it\n",
	       (unsigned)(S.cut_t - S.short_t));
}

/* (c) inrush inside the blanking, at the feed start and at every VIN-on edge of the
 * signature, never trips */
static void inrush_inside_blank_no_trip(void)
{
	sim_init(POD_INRUSH);
	sim_run(10000);
	CHECK(S.edges >= 3); /* feed start and the two signature pulses */
	CHECK(S.sm.sig_done);
	CHECK_EQ(S.trips, 0);
	CHECK(!S.sm.fault);
	CHECK(S.out.vin_en);
}

const struct check_case fast_cases[] = {
	{"fast: trips on the second reading over", trips_on_second_reading},
	{"fast: one spike does not trip", one_spike_does_not_trip},
	{"fast: a clipped reading counts as over", saturation_counts_as_over},
	{"fast: disarmed never trips", disarmed_never_trips_and_resets},
	{"fast: cut within 50 ms of a short", worst_case_within_50_ms},
	{"fast: the state machine treats it as a fault", fast_trip_is_a_fault},
	{"fast: a short from the feed start is cut after the blanking", short_from_feed_start},
	{"fast: a short in the signature pulses is cut", short_in_signature_pulse},
	{"fast: inrush inside the blanking does not trip", inrush_inside_blank_no_trip},
	{0, 0},
};
