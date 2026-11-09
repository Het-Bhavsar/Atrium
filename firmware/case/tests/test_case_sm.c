/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * The case state machine against a model pod, ticked the way main.c does: readings taken
 * with the outputs of the step before. The case's VIN goes into the pod firmware's own
 * pattern detector (firmware/src/power/vinpat.c), so the soft reset and the signature are
 * checked end to end. Every tick also checks:
 *  - the 1.8 V side is on only with VIN on for at least 100 ms;
 *  - the boost runs only with VIN on and no USB;
 *  - the zero is taken only with VIN off;
 *  - SWD runs only with the 1.8 V side on.
 */
#include <string.h>

#include "case_sm.h"
#include "check.h"
#include "vinpat.h"

struct pod {
	bool present;
	int32_t ua;       /* drawn from VIN */
	int32_t first_ua; /* at the first reading after VIN on (inrush), if set */
	bool sat;
};

struct rig {
	struct case_sm sm;
	struct case_in in;
	struct case_out out;
	uint32_t now;
	struct pod pod;
	uint32_t vin_rise;
	unsigned since_vin; /* readings since VIN came on */
	unsigned zeros;
	unsigned swd_runs;
	bool swd_ok;
	unsigned violations;
	struct vinpat pv;
	unsigned pod_resets;
	unsigned pod_case;
	unsigned vin_offs;   /* VIN falling edges */
	uint32_t off_t[16];  /* their times */
	uint32_t on_t[16];
	unsigned max_ldo_off_after_on; /* worst gap: VIN on to LDO on */
};

static struct rig R;

static void rig_init(void)
{
	static const struct case_cfg cfg = CASE_CFG_DEFAULT;
	static const struct vinpat_cfg pcfg = VINPAT_CFG_DEFAULT;

	memset(&R, 0, sizeof(R));
	R.now = 1000;
	case_sm_init(&R.sm, &cfg, R.now);
	vinpat_init(&R.pv, &pcfg, false);
	R.in.vbat_mv = 3900;
	R.in.temp_c10 = CASE_TEMP_NONE;
	R.swd_ok = true;
}

static void tick(void)
{
	const struct case_out prev = R.out;
	struct case_in *in = &R.in;

	in->now_ms = R.now;
	in->i_valid = prev.opa_on && prev.vin_en;
	in->i_ua = 0;
	in->i_sat = false;
	if (in->i_valid) {
		R.since_vin++;
		if (R.pod.present) {
			in->i_ua = R.pod.first_ua && R.since_vin == 1 ? R.pod.first_ua : R.pod.ua;
			in->i_sat = R.pod.sat;
		}
	}
	in->pod_pullup = prev.ldo_pwr && R.pod.present;
	in->swd_done = prev.swd_reset;
	in->swd_ok = R.swd_ok;
	if (prev.swd_reset) {
		R.swd_runs++;
	}

	case_sm_step(&R.sm, in, &R.out);
	struct case_out *o = &R.out;

	if (o->zero_req) {
		R.zeros++;
		if (prev.vin_en) {
			R.violations++;
			printf("  zero with VIN on at %u\n", R.now);
		}
	}
	if (o->vin_en && !prev.vin_en) {
		R.vin_rise = R.now;
		R.since_vin = 0;
		if (R.vin_offs && R.vin_offs <= 16) {
			R.on_t[R.vin_offs - 1] = R.now;
		}
	}
	if (!o->vin_en && prev.vin_en) {
		if (R.vin_offs < 16) {
			R.off_t[R.vin_offs] = R.now;
		}
		R.vin_offs++;
	}
	if (o->ldo_pwr && (!o->vin_en || R.now - R.vin_rise < 100)) {
		R.violations++;
		printf("  1.8 V side on %u ms after VIN at %u\n", R.now - R.vin_rise, R.now);
	}
	if (o->boost_en && (!o->vin_en || in->usb)) {
		R.violations++;
		printf("  boost on without need at %u\n", R.now);
	}
	if (o->swd_reset && !o->ldo_pwr) {
		R.violations++;
		printf("  SWD without the 1.8 V side at %u\n", R.now);
	}
	/* the pod sees VIN only when it sits on the pins */
	bool pod_vin = o->vin_en && R.pod.present;
	enum vinpat_ev ev = vinpat_edge(&R.pv, pod_vin, R.now);

	if (ev == VINPAT_RESET) {
		R.pod_resets++;
	} else if (ev == VINPAT_CASE) {
		R.pod_case++;
	}
	R.now += CASE_TICK_MS;
}

/* run for ms: 100 ms ticks while awake; in standby, AWU wake-ups as main.c sets them:
 * the next timer rounded up to 480 ms, at most 30.72 s */
static void run(uint32_t ms)
{
	uint32_t end = R.now + ms;

	while ((int32_t)(end - R.now) > 0) {
		if (R.out.awake) {
			tick();
		} else if (R.out.wake_in_ms) {
			uint32_t n = (R.out.wake_in_ms + 479) / 480;
			uint32_t step = (n > 64 ? 64 : n) * 480;

			/* stop at the end of the run, as an early wake-up would: no overshoot that
			 * adds up across runs */
			R.now += step < end - R.now ? step : end - R.now;
			tick();
		} else {
			R.now = end;
		}
	}
}

/* one tick for an input change (an EXTI wake-up) then run */
static void after(uint32_t ms)
{
	tick();
	run(ms);
}

static void boot_no_pod_sleeps(void)
{
	rig_init();
	after(3000);
	CHECK_EQ(R.zeros, 1);
	CHECK_EQ(R.sm.pod, POD_NONE);
	CHECK(!R.out.vin_en);
	CHECK(!R.out.awake);
	CHECK_EQ(R.out.wake_in_ms, 0);
	CHECK_EQ(R.violations, 0);
}

static void charging_pod_on_lid_open(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	after(1000);
	CHECK_EQ(R.sm.pod, POD_CHARGING);
	R.in.lid_open = true;
	after(300);
	CHECK_EQ(R.out.led, LED_POD_CHARGING);
	CHECK(R.out.vin_en && R.out.boost_en && R.out.ldo_pwr);
	R.in.lid_open = false;
	after(60000);
	CHECK(R.out.vin_en); /* still charging with the lid shut */
	CHECK(R.out.awake);
	CHECK_EQ(R.violations, 0);
}

static void full_then_off_then_topup(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	after(60000);
	CHECK_EQ(R.sm.pod, POD_CHARGING);
	R.pod.ua = 1500; /* taper, then the charger's own done */
	run(29000);
	CHECK(R.out.vin_en);
	CHECK_EQ(R.sm.pod, POD_CHARGING);
	run(2000);
	CHECK_EQ(R.sm.pod, POD_FULL);
	run(500);
	CHECK(!R.out.vin_en);
	CHECK(!R.out.awake);
	CHECK(R.out.wake_in_ms > 3UL * 3600 * 1000);
	unsigned zeros = R.zeros;

	run(4UL * 3600 * 1000 - 60000);
	CHECK(!R.out.vin_en);
	CHECK_EQ(R.zeros, zeros);
	run(120000);
	CHECK_EQ(R.zeros, zeros + 1); /* the top-up fed it again */
	run(60000);
	CHECK(!R.out.vin_en); /* confirmed full again */
	CHECK_EQ(R.sm.pod, POD_FULL);

	/* a top-up that finds the pod charging (it ran down) stays on */
	R.pod.ua = 12000;
	run(4UL * 3600 * 1000 + 2000);
	CHECK(R.out.vin_en);
	CHECK_EQ(R.sm.pod, POD_CHARGING);
	CHECK_EQ(R.violations, 0);
}

static void full_on_usb_stays_fed(void)
{
	rig_init();
	R.in.usb = true;
	R.in.chrg = true;
	R.pod = (struct pod){.present = true, .ua = 1000};
	after(40000);
	CHECK_EQ(R.sm.pod, POD_FULL);
	run(60000);
	CHECK(R.out.vin_en);
	CHECK(!R.out.boost_en);
	CHECK_EQ(R.out.led, LED_USB_CHARGING);
	R.in.chrg = false;
	after(200);
	CHECK_EQ(R.out.led, LED_USB_FULL);
	R.in.usb = false;
	after(1000);
	CHECK(!R.out.vin_en); /* full and settled: off without USB */
	CHECK_EQ(R.out.led, LED_OFF);
	CHECK_EQ(R.violations, 0);
}

static void full_pod_green_at_once(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 1000};
	after(40000);
	CHECK(!R.out.vin_en);
	R.in.lid_open = true;
	tick();
	CHECK_EQ(R.out.led, LED_POD_FULL);
	CHECK(R.out.vin_en); /* the lid open feeds, to see a removal */
	run(5500);
	CHECK_EQ(R.out.led, LED_OFF);
	CHECK_EQ(R.violations, 0);
}

static void no_pod_shows_battery(void)
{
	static const struct {
		uint16_t mv;
		enum led_pat want;
	} lv[] = {{4100, LED_BATT_HIGH}, {3750, LED_BATT_MID}, {3600, LED_BATT_LOW}};

	for (unsigned i = 0; i < 3; i++) {
		rig_init();
		R.in.vbat_mv = lv[i].mv;
		after(3000);
		R.in.lid_open = true;
		tick();
		CHECK_EQ(R.out.led, lv[i].want);
		R.in.lid_open = false;
		after(3000);
	}
}

static void lid_idle_stops_feeding(void)
{
	rig_init();
	after(3000);
	R.in.lid_open = true;
	after(60000);
	CHECK(R.out.vin_en);
	run(61000);
	CHECK(!R.out.vin_en);
	CHECK(!R.out.awake);
	/* a pod put in with VIN off is found when the lid shuts */
	R.pod = (struct pod){.present = true, .ua = 15000};
	R.in.lid_open = false;
	after(1000);
	CHECK(R.out.vin_en);
	CHECK_EQ(R.sm.pod, POD_CHARGING);
	CHECK_EQ(R.violations, 0);
}

static void removal_is_seen(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	R.in.lid_open = true;
	after(5000);
	CHECK_EQ(R.sm.pod, POD_CHARGING);
	R.pod.present = false;
	run(1500);
	CHECK_EQ(R.sm.pod, POD_CHARGING);
	run(1000);
	CHECK_EQ(R.sm.pod, POD_NONE);
	R.in.lid_open = false;
	after(3000);
	CHECK(!R.out.vin_en);
	CHECK_EQ(R.violations, 0);
}

static void present_by_pullup_alone(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 100}; /* idle, below 0.4 mA */
	after(10000);
	CHECK(case_pod_present(&R.sm));
	CHECK(R.out.vin_en);
	CHECK_EQ(R.violations, 0);
}

static void over_current_trips_and_retries(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 100000};
	after(500);
	CHECK(!R.out.vin_en);
	CHECK(R.sm.fault);
	CHECK_EQ(R.out.led, LED_FAULT);
	CHECK_EQ(R.vin_offs, 1);
	run(29000);
	CHECK_EQ(R.vin_offs, 1);
	run(1600);
	CHECK_EQ(R.vin_offs, 2); /* retried at 30 s, tripped again */
	R.pod.ua = 15000;
	run(31000);
	CHECK(R.out.vin_en);
	CHECK(!R.sm.fault);
	CHECK_EQ(R.violations, 0);

	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000, .sat = true};
	after(500);
	CHECK(R.sm.fault);
}

static void inrush_is_blanked(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000, .first_ua = 300000};
	after(2000);
	CHECK(!R.sm.fault);
	CHECK(R.out.vin_en);
}

static void low_battery(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	after(2000);
	R.in.vbat_mv = 3380;
	run(200);
	CHECK(R.out.vin_en); /* two readings */
	run(200);
	CHECK(!R.out.vin_en);
	R.in.lid_open = true;
	tick();
	CHECK_EQ(R.out.led, LED_LOWBAT);
	R.in.lid_open = false;
	R.in.vbat_mv = 3500;
	after(10000);
	CHECK(!R.out.vin_en); /* hysteresis */
	R.in.usb = true;
	after(500);
	CHECK(R.out.vin_en);
	CHECK(!R.out.boost_en);
	R.in.usb = false;
	R.in.vbat_mv = 3650;
	after(1000);
	CHECK(R.out.vin_en);
	CHECK_EQ(R.violations, 0);
}

static void hot_cell(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	R.in.temp_c10 = 300;
	after(2000);
	R.in.temp_c10 = 455;
	tick();
	CHECK(!R.out.vin_en);
	R.in.temp_c10 = CASE_TEMP_NONE; /* the NTC is unpowered with VIN off */
	run(590000);
	CHECK(!R.out.vin_en);
	run(20000);
	CHECK(R.out.vin_en); /* retry at 10 min */
	R.in.temp_c10 = 430;  /* above the resume threshold */
	tick();
	CHECK(!R.out.vin_en);
	R.in.temp_c10 = CASE_TEMP_NONE;
	run(610000);
	R.in.temp_c10 = 410;
	after(5000);
	CHECK(R.out.vin_en);
	R.in.temp_c10 = 440; /* below 45 C after a reading below 42 C */
	after(1000);
	CHECK(R.out.vin_en);
	CHECK_EQ(R.violations, 0);
}

static void press(uint32_t ms)
{
	R.in.btn = true;
	after(ms);
	R.in.btn = false;
	tick();
}

static void short_press(void)
{
	rig_init();
	R.in.vbat_mv = 4100;
	R.pod = (struct pod){.present = true, .ua = 15000};
	after(1000);
	press(200);
	CHECK_EQ(R.out.led, LED_POD_CHARGING);
	run(5000);
	CHECK_EQ(R.out.led, LED_BATT_HIGH);
	run(1600);
	CHECK_EQ(R.out.led, LED_OFF);

	/* no pod: the battery only */
	rig_init();
	R.in.vbat_mv = 3750;
	after(3000);
	press(200);
	CHECK_EQ(R.out.led, LED_BATT_MID);
}

/* VIN off stretches between `from` and the end, in ms */
static unsigned offs_since(unsigned from)
{
	return R.vin_offs - from;
}

static void soft_reset(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	after(10000);
	CHECK_EQ(R.pod_case, 1); /* the signature */
	CHECK_EQ(R.pod_resets, 0);
	unsigned before = R.vin_offs;

	R.in.btn = true;
	after(5000);
	CHECK_EQ(R.out.led, LED_SOFT_RESET);
	run(1000);
	R.in.btn = false;
	after(3000);
	CHECK_EQ(offs_since(before), 3);
	CHECK_EQ(R.pod_resets, 1);
	for (unsigned i = before; i < before + 3; i++) {
		CHECK_EQ(R.on_t[i] - R.off_t[i], 200); /* 200 ms off */
	}
	CHECK(R.on_t[before + 2] - R.off_t[before] <= 2000);
	CHECK(R.out.vin_en);
	CHECK_EQ(R.sm.pod, POD_CHARGING); /* the pod state held through the pattern */
	run(5000);
	CHECK_EQ(R.pod_case, 3); /* the pattern's second pulse, then a new signature */
	CHECK_EQ(R.pod_resets, 1);
	CHECK(vinpat_on_case(&R.pv));
	CHECK_EQ(R.violations, 0);
}

static void soft_reset_needs_a_pod(void)
{
	rig_init();
	after(3000);
	unsigned before = R.vin_offs;

	press(6000);
	run(5000);
	CHECK_EQ(offs_since(before), 0);
	CHECK_EQ(R.pod_resets, 0);
}

static void hard_reset(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	after(10000);
	R.in.btn = true;
	after(5000);
	CHECK_EQ(R.out.led, LED_SOFT_RESET);
	run(7000);
	CHECK_EQ(R.out.led, LED_HARD_RESET);
	run(500);
	R.in.btn = false;
	after(1000);
	CHECK_EQ(R.swd_runs, 1);
	CHECK_EQ(R.pod_resets, 0); /* no VIN pattern for a hard reset */
	run(5000);
	CHECK_EQ(R.swd_runs, 1);
	CHECK_EQ(R.pod_case, 2); /* the signature again after the reset */

	/* a failed hard reset shows red */
	R.swd_ok = false;
	press(13000);
	run(300);
	CHECK_EQ(R.out.led, LED_SWD_FAIL);
	CHECK_EQ(R.swd_runs, 2);
	CHECK_EQ(R.violations, 0);
}

static void hard_reset_with_vin_off(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 1000};
	after(40000);
	CHECK(!R.out.vin_en); /* full, settled */
	press(12500);
	CHECK(R.out.vin_en);
	CHECK_EQ(R.swd_runs, 0);
	run(1000);
	CHECK_EQ(R.swd_runs, 1);
	CHECK_EQ(R.violations, 0);
}

static void a_day_without_resets(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	after(3600000UL);
	R.pod.ua = 1000;
	for (int h = 0; h < 24; h++) {
		run(3600000UL);
		R.in.lid_open = true;
		after(20000);
		R.in.lid_open = false;
		after(2000);
	}
	CHECK_EQ(R.pod_resets, 0);
	CHECK(R.pod_case > 10);
	CHECK_EQ(R.violations, 0);
}

static void ldo_waits_for_vin(void)
{
	rig_init();
	R.pod = (struct pod){.present = true, .ua = 15000};
	tick();
	CHECK(R.out.vin_en);
	CHECK(!R.out.ldo_pwr);
	tick(); /* 100 ms */
	CHECK(R.out.ldo_pwr);
	CHECK_EQ(R.violations, 0);
}

const struct check_case sm_cases[] = {
	{"sm: boot with no pod sleeps", boot_no_pod_sleeps},
	{"sm: charging pod on lid open", charging_pod_on_lid_open},
	{"sm: full, VIN off, top-up", full_then_off_then_topup},
	{"sm: full on USB stays fed", full_on_usb_stays_fed},
	{"sm: full pod shows green at once", full_pod_green_at_once},
	{"sm: no pod shows the battery", no_pod_shows_battery},
	{"sm: lid open idle stops feeding", lid_idle_stops_feeding},
	{"sm: pod removal is seen", removal_is_seen},
	{"sm: present by the SWDIO pull-up", present_by_pullup_alone},
	{"sm: over-current trips and retries", over_current_trips_and_retries},
	{"sm: inrush is blanked", inrush_is_blanked},
	{"sm: low case battery", low_battery},
	{"sm: hot case cell", hot_cell},
	{"sm: short press", short_press},
	{"sm: soft reset reboots the pod", soft_reset},
	{"sm: soft reset needs a pod", soft_reset_needs_a_pod},
	{"sm: hard reset", hard_reset},
	{"sm: hard reset with VIN off", hard_reset_with_vin_off},
	{"sm: a day of top-ups and lid opens", a_day_without_resets},
	{"sm: 1.8 V side waits for VIN", ldo_waits_for_vin},
	CASES_END,
};
