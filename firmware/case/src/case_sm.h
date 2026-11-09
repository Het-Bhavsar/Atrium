/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The charging case's behaviour (docs/charging-case.md), without hardware. main.c calls
 * case_sm_step() every 100 ms while awake, and once on each wake-up, with what it measured;
 * it then sets the outputs. Plain C, unit-tested on the host (case/tests).
 *
 * Feeding the pod (VIN on)
 *  - VIN goes on when the lid opens, when a pod is charging, on USB, and for a top-up of a
 *    full pod every 4 h [ASSUMED]. Before VIN goes on, main zeroes the current reading
 *    (zero_req). Readings until 100 ms after VIN goes on are ignored (pod input capacitors).
 *  - The translators' 1.8 V side (LDO_PWR) goes on 100 ms after VIN, and off with VIN, so
 *    an unpowered nRF52832 is never driven through its pins.
 *  - Pod present: SWDIO reads high (the nRF's pull-up against a 1M pull-down on the case
 *    side [VERIFY]) or the current is above 0.4 mA. Absent for 2 s: no pod.
 *  - Charging above 5 mA; full below 2.5 mA for 30 s with the pod present. Between the two
 *    the state holds. Once full without USB and with the lid shut, VIN goes off.
 *  - Over 80 mA for 3 readings (or the reading clips): contact fault, VIN off, retry 30 s.
 *  - Case cell below 3.4 V without USB (3 readings): no feeding until 3.6 V or USB.
 *  - Case cell above 45 C (NTC, read only while feeding): VIN off, retry in 10 min; it
 *    stays off until a reading below 42 C.
 *  - Case signature: 3 s after VIN goes on with a pod there, and again after a reset, two
 *    200 ms VIN off pulses. The pod counts them on /PG and reports "fed by the case"
 *    (src/power/vinpat.h); three within 2 s is the soft reset.
 * Button: a short press shows the pod, then the case battery. Holding marks 5 s with a
 * white flash and 12 s with a double flash; the action runs on release, with a pod docked:
 * 5-12 s soft reset (3 x 200 ms VIN off within 2 s), 12 s or more hard reset (SWD CTRL-AP).
 * The MCU may go to standby once nothing needs it; wake_in_ms asks for an AWU wake-up.
 */
#ifndef CASE_SM_H_
#define CASE_SM_H_

#include <stdbool.h>
#include <stdint.h>

#include "led.h"

#define CASE_TICK_MS 100
#define CASE_TEMP_NONE INT16_MIN

struct case_cfg {
	uint16_t blank_ms;      /* after VIN on, before readings count */
	uint16_t ldo_delay_ms;  /* VIN on to the 1.8 V side on */
	int32_t chg_ua;         /* above: charging */
	int32_t full_ua;        /* below, for full_ms: full */
	uint32_t full_ms;
	int32_t present_ua;     /* above: a pod is there */
	uint32_t absent_ms;     /* no pod seen for this long: none */
	int32_t oc_ua;          /* over-current */
	uint8_t oc_count;       /* readings in a row */
	uint32_t retry_ms;      /* after a fault */
	uint16_t vbat_cut_mv;
	uint16_t vbat_resume_mv;
	uint8_t vbat_count;     /* readings in a row below the cut */
	int16_t hot_c10;
	int16_t hot_resume_c10;
	uint32_t hot_retry_ms;
	uint32_t topup_ms;      /* a full pod, VIN off: feed again after this */
	uint32_t lid_idle_ms;   /* lid open, no pod: stop feeding after this */
	uint32_t short_ms;      /* a press shorter than this shows the status */
	uint32_t soft_ms;       /* hold for the soft reset */
	uint32_t hard_ms;       /* hold for the hard reset */
	uint16_t pulse_off_ms;  /* soft reset: VIN off this long, 3 times */
	uint16_t pulse_on_ms;   /* and on this long between */
	uint16_t pre_pulse_ms;  /* VIN steady at least this long before a pattern (clear of the
				 * pod's 2 s window after the last one) */
	uint16_t sig_at_ms;     /* VIN on (or a reset) to the case signature */
	uint16_t swd_ready_ms;  /* 1.8 V side on at least this long before SWD */
	uint16_t show_wait_ms;  /* wait this long for the pod's state before showing it */
};

#define CASE_CFG_DEFAULT                                                                       \
	{                                                                                      \
		.blank_ms = 100, .ldo_delay_ms = 100, .chg_ua = 5000, .full_ua = 2500,         \
		.full_ms = 30000, .present_ua = 400, .absent_ms = 2000, .oc_ua = 80000,        \
		.oc_count = 3, .retry_ms = 30000, .vbat_cut_mv = 3400, .vbat_resume_mv = 3600, \
		.vbat_count = 3, .hot_c10 = 450, .hot_resume_c10 = 420,                        \
		.hot_retry_ms = 600000, .topup_ms = 4UL * 3600 * 1000, .lid_idle_ms = 120000,  \
		.short_ms = 1000, .soft_ms = 5000, .hard_ms = 12000, .pulse_off_ms = 200,      \
		.pulse_on_ms = 300, .pre_pulse_ms = 2500, .sig_at_ms = 3000,         \
		.swd_ready_ms = 100,                  \
		.show_wait_ms = 2500,                                                          \
	}

enum pod_state { POD_UNKNOWN = 0, POD_NONE, POD_CHARGING, POD_FULL };

struct case_in {
	uint32_t now_ms;
	bool lid_open;
	bool btn;          /* pressed */
	bool usb;          /* VBUS present */
	bool chrg;         /* TP4054 CHRG low: the case cell is charging */
	uint16_t vbat_mv;  /* 0: not measured this time */
	int16_t temp_c10;  /* CASE_TEMP_NONE: not measured (only while feeding) */
	bool i_valid;      /* a pod current reading this time */
	int32_t i_ua;
	bool i_sat;        /* the reading clipped */
	bool pod_pullup;   /* SWDIO read high (valid while ldo_pwr) */
	bool swd_done;     /* the hard reset asked for last time has run */
	bool swd_ok;
};

struct case_out {
	bool vin_en;
	bool boost_en;
	bool ldo_pwr;
	bool opa_on;      /* OPA and shunt readings wanted */
	bool zero_req;    /* take the zero reading now, VIN still off, then set vin_en */
	bool swd_reset;   /* run the hard reset now (once) */
	enum led_pat led;
	uint32_t led_t0;  /* the pattern's start, in now_ms */
	bool awake;       /* keep ticking at 100 ms; false: standby is fine */
	uint32_t wake_in_ms; /* standby: wake within this (0: only on lid, button, USB) */
};

struct case_sm {
	struct case_cfg cfg;
	enum pod_state pod;
	bool fault;
	bool lowbat;
	bool hot;
	bool was_hot;
	bool settled;      /* full confirmed in this feed */
	bool feed;         /* VIN on (outside the soft-reset off pulses) */
	uint32_t feed_t0;
	bool absent_on;
	uint32_t absent_t0;
	bool low_on;
	uint32_t low_t0;
	uint8_t oc;
	uint8_t vlow;
	uint32_t retry_at;
	uint32_t hot_at;
	uint32_t topup_at;
	bool lid;
	uint32_t lid_t0;
	bool btn;
	uint32_t btn_t0;
	uint8_t marks;     /* hold thresholds shown: 1 soft, 2 hard */
	bool pulse_want;
	bool pulse;
	uint8_t pulse_n;   /* 2 signature, 3 soft reset */
	uint32_t pulse_t0;
	bool sig_done;
	uint32_t sig_t0;   /* the signature is due sig_at_ms after this */
	bool swd_want;
	bool swd_busy;
	bool show;
	bool show_batt;    /* battery after the pod */
	uint32_t show_t0;
	enum led_pat led;
	enum led_pat led_next;
	uint32_t led_t0;
	uint16_t vbat_mv;
};

void case_sm_init(struct case_sm *sm, const struct case_cfg *cfg, uint32_t now_ms);
void case_sm_step(struct case_sm *sm, const struct case_in *in, struct case_out *out);

/* What the LED would show for the pod right now (for tests and the debug print). */
bool case_pod_present(const struct case_sm *sm);

#endif /* CASE_SM_H_ */
