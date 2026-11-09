/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "epoch.h"

#include <string.h>

#define CLS_WALK 1
#define CLS_RUN  2

void epoch_open(struct epoch *e, uint32_t t0)
{
	memset(e, 0, sizeof(*e));
	e->t0 = t0;
}

uint32_t epoch_next_t0(uint32_t prev_t0, bool have_prev, uint32_t now)
{
	uint32_t want = prev_t0 + LOG_EPOCH_S;
	int32_t d = (int32_t)(now - want);

	if (have_prev && d >= -2 && d <= 2 && ((now ^ want) & LOG_TIME_UPTIME) == 0) {
		return want;
	}
	return now;
}

void epoch_second(struct epoch *e, const struct epoch_second *s)
{
	if (e->n == UINT8_MAX) {
		return;
	}
	e->n++;
	e->undocked += !s->docked;
	e->worn += s->on_skin;
	e->pulse_s += s->pulse_present && s->on_skin;
	e->docked |= s->docked;
	e->charging |= s->charging;
	e->pulse_ir |= s->pulse_ir && s->on_skin;
	if (s->bpm) {
		e->bpm_n++;
		e->bpm_sum += s->bpm;
		e->conf_sum += s->conf;
	}
}

bool epoch_close(const struct epoch *e, const struct epoch_close_in *in, struct log_record *out)
{
	if (e->undocked == 0) {
		return false;
	}
	memset(out, 0, sizeof(*out));
	if (e->bpm_n) {
		out->bpm = (float)e->bpm_sum / e->bpm_n;
		out->conf = e->conf_sum / e->bpm_n;
	}
	out->on_skin = 2U * e->worn > e->n;
	out->pulse_ir = e->pulse_ir;
	out->docked = e->docked;
	out->charging = e->charging;
	out->rmssd_ms = in->hrv.rmssd_x10 / 10.0f;
	out->n_diffs = in->hrv.n_diffs;
	out->cls = in->cls & 0x03;
	out->activity_mg = in->activity_mg;
	out->mlc_max = in->mlc_max;
	out->pulse_s = e->pulse_s > 30 ? 30 : e->pulse_s;
	out->skin_cc = in->skin_valid && in->skin_cc > INT16_MIN && in->skin_cc <= INT16_MAX
			       ? (int16_t)in->skin_cc
			       : LOG_SKIN_UNKNOWN;
	out->steps = (out->cls == CLS_WALK || out->cls == CLS_RUN) ? (uint16_t)(in->steps_pm / 2U) : 0;
	out->pitch_deg = in->pitch_deg;
	out->roll_deg = in->roll_deg;
	if (in->ox && in->ox_r > 0.0f) {
		out->ox_r = in->ox_r;
		out->ox_conf = in->ox_conf;
	}
	return true;
}
