/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "case_sm.h"

#include <string.h>

#include "sense.h"

/* times are ms and wrap after 49 days: compare by difference */
static bool due(uint32_t now, uint32_t at)
{
	return (int32_t)(now - at) >= 0;
}

static uint32_t left(uint32_t now, uint32_t at)
{
	return due(now, at) ? 1 : at - now;
}

void case_sm_init(struct case_sm *sm, const struct case_cfg *cfg, uint32_t now_ms)
{
	memset(sm, 0, sizeof(*sm));
	sm->cfg = *cfg;
	sm->pod = POD_UNKNOWN; /* probe once at power-up */
	sm->lid_t0 = now_ms;
	sm->led = LED_OFF;
	sm->led_next = LED_OFF;
}

bool case_pod_present(const struct case_sm *sm)
{
	return sm->pod == POD_CHARGING || sm->pod == POD_FULL;
}

static void led_play(struct case_sm *sm, enum led_pat p, enum led_pat next, uint32_t now)
{
	sm->led = p;
	sm->led_next = next;
	sm->led_t0 = now;
}

static enum led_pat pod_pattern(const struct case_sm *sm)
{
	if (sm->fault) {
		return LED_FAULT;
	}
	if (sm->lowbat) {
		return LED_LOWBAT;
	}
	switch (sm->pod) {
	case POD_CHARGING:
		return LED_POD_CHARGING;
	case POD_FULL:
		return LED_POD_FULL;
	default:
		return led_batt(sense_batt_pct(sm->vbat_mv)); /* no pod: the case battery */
	}
}

static void inputs(struct case_sm *sm, const struct case_in *in)
{
	const struct case_cfg *c = &sm->cfg;
	uint32_t now = in->now_ms;

	if (in->vbat_mv) {
		sm->vbat_mv = in->vbat_mv;
		if (in->vbat_mv < c->vbat_cut_mv) {
			sm->vlow = sm->vlow < UINT8_MAX ? sm->vlow + 1 : sm->vlow;
		} else {
			sm->vlow = 0;
		}
		if (sm->vlow >= c->vbat_count) {
			sm->lowbat = true;
		}
		if (in->vbat_mv >= c->vbat_resume_mv) {
			sm->lowbat = false;
		}
	}
	if (in->usb) {
		sm->lowbat = false;
		sm->vlow = 0;
	}

	/* the NTC is powered from VIN_EN, so a reading means VIN was on */
	if (sm->feed && in->temp_c10 != CASE_TEMP_NONE) {
		int16_t limit = sm->was_hot ? c->hot_resume_c10 : c->hot_c10;

		if (in->temp_c10 >= limit) {
			sm->hot = true;
			sm->was_hot = true;
			sm->hot_at = now + c->hot_retry_ms;
		} else if (in->temp_c10 < c->hot_resume_c10) {
			sm->was_hot = false;
		}
	}

	/* button: marks while held, the action on release */
	if (in->btn && !sm->btn) {
		sm->btn_t0 = now;
		sm->marks = 0;
	}
	if (in->btn && case_pod_present(sm)) {
		uint32_t held = now - sm->btn_t0;

		if (sm->marks < 1 && held >= c->soft_ms) {
			led_play(sm, LED_SOFT_RESET, LED_OFF, now);
			sm->marks = 1;
		}
		if (sm->marks < 2 && held >= c->hard_ms) {
			led_play(sm, LED_HARD_RESET, LED_OFF, now);
			sm->marks = 2;
		}
	}
	if (!in->btn && sm->btn) {
		uint32_t held = now - sm->btn_t0;

		if (case_pod_present(sm) && held >= c->hard_ms) {
			sm->swd_want = true;
		} else if (case_pod_present(sm) && held >= c->soft_ms) {
			sm->pulse_want = true;
		} else if (held < c->short_ms) {
			sm->show = true;
			sm->show_batt = true;
			sm->show_t0 = now;
		}
	}
	sm->btn = in->btn;

	/* lid */
	if (in->lid_open && !sm->lid) {
		sm->lid_t0 = now;
		sm->show = true;
		sm->show_batt = false;
		sm->show_t0 = now;
	}
	if (!in->lid_open && sm->lid && sm->pod == POD_NONE) {
		sm->pod = POD_UNKNOWN; /* a pod may have gone in while VIN was off: look again */
	}
	sm->lid = in->lid_open;

	/* timers */
	if (sm->fault && due(now, sm->retry_at)) {
		sm->fault = false;
	}
	if (sm->hot && due(now, sm->hot_at)) {
		sm->hot = false;
	}
	if (sm->pod == POD_FULL && sm->settled && !in->usb && due(now, sm->topup_at)) {
		sm->settled = false; /* top-up */
	}
}

/* a pulse train: n times VIN off, with VIN on between, then on for good */
static bool pulse_vin(const struct case_sm *sm, uint32_t now, bool *done)
{
	const struct case_cfg *c = &sm->cfg;
	uint32_t e = now - sm->pulse_t0;
	uint32_t period = c->pulse_off_ms + c->pulse_on_ms;

	*done = e >= (uint32_t)sm->pulse_n * c->pulse_off_ms + (sm->pulse_n - 1u) * c->pulse_on_ms;
	return *done || e % period >= c->pulse_off_ms;
}

static void pulse_start(struct case_sm *sm, uint8_t n, uint32_t now)
{
	sm->pulse = true;
	sm->pulse_n = n;
	sm->pulse_t0 = now;
}

static void oc_fault(struct case_sm *sm, uint32_t now)
{
	sm->fault = true;
	sm->oc = 0;
	sm->retry_at = now + sm->cfg.retry_ms;
	sm->feed = false;
	sm->pulse = false;
	sm->pulse_want = false;
	sm->swd_want = false;
	led_play(sm, LED_FAULT, LED_OFF, now);
}

static void measure(struct case_sm *sm, const struct case_in *in, bool ldo_seen)
{
	const struct case_cfg *c = &sm->cfg;
	uint32_t now = in->now_ms;

	if (in->i_sat || in->i_ua > c->oc_ua) {
		if (++sm->oc >= c->oc_count) {
			oc_fault(sm, now);
			return;
		}
	} else {
		sm->oc = 0;
	}

	bool here = (ldo_seen && in->pod_pullup) || in->i_ua >= c->present_ua;

	if (!here) {
		if (!sm->absent_on) {
			sm->absent_on = true;
			sm->absent_t0 = now;
		}
		sm->low_on = false;
		if (now - sm->absent_t0 >= c->absent_ms) {
			sm->pod = POD_NONE;
		}
		return;
	}
	sm->absent_on = false;
	if (in->i_ua > c->chg_ua) {
		sm->pod = POD_CHARGING;
		sm->low_on = false;
	} else if (in->i_ua < c->full_ua) {
		if (!sm->low_on) {
			sm->low_on = true;
			sm->low_t0 = now;
		}
		if (now - sm->low_t0 >= c->full_ms) {
			if (sm->pod != POD_FULL || !sm->settled) {
				sm->pod = POD_FULL;
				sm->settled = true;
				sm->topup_at = now + c->topup_ms;
			}
		} else if (sm->pod == POD_UNKNOWN || sm->pod == POD_NONE) {
			sm->pod = POD_CHARGING; /* present; full is shown once proved */
		}
	} else {
		sm->low_on = false;
		if (sm->pod == POD_UNKNOWN || sm->pod == POD_NONE) {
			sm->pod = POD_CHARGING;
		}
	}
}

void case_sm_step(struct case_sm *sm, const struct case_in *in, struct case_out *out)
{
	const struct case_cfg *c = &sm->cfg;
	uint32_t now = in->now_ms;
	bool ldo_seen = sm->feed && !sm->pulse && now - sm->feed_t0 >= (uint32_t)c->ldo_delay_ms + CASE_TICK_MS;

	memset(out, 0, sizeof(*out));
	inputs(sm, in);

	/* the hard reset ran */
	if (sm->swd_busy && in->swd_done) {
		sm->swd_busy = false;
		sm->sig_done = false; /* the pod rebooted: tell it again */
		sm->sig_t0 = now;
		if (!in->swd_ok) {
			led_play(sm, LED_SWD_FAIL, LED_OFF, now);
		}
	}

	/* main already cut VIN between ticks for an over-current */
	if (in->fast_trip && sm->feed && !sm->fault) {
		oc_fault(sm, now);
	}

	/* readings while VIN has been on past the blanking, outside the soft-reset pattern */
	if (sm->feed && !sm->pulse && now - sm->feed_t0 > c->blank_ms && in->i_valid) {
		measure(sm, in, ldo_seen);
	}

	/* feed or not */
	bool blocked = sm->fault || sm->hot || (sm->lowbat && !in->usb);
	bool lid_feed = in->lid_open && (case_pod_present(sm) || now - sm->lid_t0 < c->lid_idle_ms);
	bool want = !blocked &&
		    (sm->pulse || sm->pulse_want || sm->swd_want || sm->swd_busy || lid_feed ||
		     sm->pod == POD_UNKNOWN || sm->pod == POD_CHARGING ||
		     (sm->pod == POD_FULL && (in->usb || !sm->settled)));

	if (blocked) {
		sm->pulse = false;
		sm->pulse_want = false;
		sm->swd_want = false;
	}
	if (want && !sm->feed) {
		sm->feed = true;
		sm->feed_t0 = now;
		sm->absent_on = false;
		sm->low_on = false;
		sm->oc = 0;
		sm->settled = false;
		sm->sig_done = false;
		sm->sig_t0 = now;
		out->zero_req = true;
	} else if (!want && sm->feed) {
		sm->feed = false;
		sm->pulse = false;
	}

	/* the case signature, then the soft reset: each clear of the pod's 2 s window */
	bool steady = sm->feed && !sm->pulse && now - sm->feed_t0 >= c->pre_pulse_ms;

	if (steady && !sm->sig_done && case_pod_present(sm) && now - sm->sig_t0 >= c->sig_at_ms) {
		pulse_start(sm, 2, now);
		sm->sig_done = true;
	} else if (steady && sm->sig_done && sm->pulse_want) {
		pulse_start(sm, 3, now);
		sm->pulse_want = false;
	}
	bool vin = sm->feed;

	if (sm->feed && sm->pulse) {
		bool done;

		vin = pulse_vin(sm, now, &done);
		if (done) {
			sm->pulse = false;
			sm->feed_t0 = now; /* blanking and the 1.8 V delay start again */
			if (sm->pulse_n == 3) {
				sm->sig_done = false; /* the pod rebooted: tell it again */
				sm->sig_t0 = now;
			}
		}
	}
	bool ldo = sm->feed && !sm->pulse && now - sm->feed_t0 >= c->ldo_delay_ms;

	/* hard reset: the 1.8 V side has been on for swd_ready_ms */
	if (sm->swd_want && ldo && now - sm->feed_t0 >= (uint32_t)c->ldo_delay_ms + c->swd_ready_ms) {
		sm->swd_want = false;
		sm->swd_busy = true;
		out->swd_reset = true;
	}

	/* what to show */
	if (sm->show) {
		bool ready = sm->pod != POD_UNKNOWN || sm->fault || sm->lowbat ||
			     now - sm->show_t0 >= c->show_wait_ms;

		if (ready) {
			enum led_pat p = pod_pattern(sm);
			bool pod_shown = p == LED_POD_CHARGING || p == LED_POD_FULL;
			enum led_pat next = sm->show_batt && pod_shown
						    ? led_batt(sense_batt_pct(sm->vbat_mv))
						    : LED_OFF;

			led_play(sm, p, next, now);
			sm->show = false;
		}
	}
	if (sm->led != LED_OFF) {
		uint32_t len = led_length(sm->led);

		if (len && now - sm->led_t0 >= len) {
			led_play(sm, sm->led_next, LED_OFF, now);
		}
	}
	out->led = sm->led;
	out->led_t0 = sm->led_t0;
	if (sm->led == LED_OFF && in->usb) {
		out->led = in->chrg ? LED_USB_CHARGING : LED_USB_FULL;
		out->led_t0 = now;
	}

	out->vin_en = vin;
	out->boost_en = vin && !in->usb;
	out->ldo_pwr = ldo;
	out->opa_on = sm->feed;
	out->oc_armed = vin && sm->feed && !sm->pulse && now - sm->feed_t0 > c->blank_ms;
	out->awake = sm->feed || in->btn || sm->show || sm->led != LED_OFF || in->usb ||
		     sm->pulse_want || sm->swd_want || sm->swd_busy;

	uint32_t w = 0;

	if (sm->fault) {
		w = left(now, sm->retry_at);
	}
	if (sm->hot && (!w || left(now, sm->hot_at) < w)) {
		w = left(now, sm->hot_at);
	}
	if (sm->pod == POD_FULL && sm->settled && !in->usb && (!w || left(now, sm->topup_at) < w)) {
		w = left(now, sm->topup_at);
	}
	out->wake_in_ms = w;
}
