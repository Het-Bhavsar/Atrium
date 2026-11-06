/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "wear.h"

#include <string.h>

void wear_init(struct wear *w, const struct wear_cfg *cfg)
{
	memset(w, 0, sizeof(*w));
	w->cfg = *cfg;
	w->dc_ref = cfg->dc_ref_default;
	wear_restart(w);
}

void wear_restart(struct wear *w)
{
	w->worn = true;
	w->dark = 0;
	w->lit = 0;
	w->since_pulse = 0;
	w->pending = -1;
	w->t_n = 0;
	w->t_head = 0;
	w->learn_sum = 0;
	w->learn_n = 0;
}

static uint16_t inc(uint16_t v)
{
	return v < UINT16_MAX ? v + 1 : v;
}

/* degrees per minute over the last 60 s (wear.py run: (T[i] - T[i-60]) * 60 / span) */
static float temp_rate(struct wear *w, float t)
{
	w->temp[w->t_head] = t;
	w->t_head = (uint8_t)((w->t_head + 1) % WEAR_TEMP_HIST);
	w->t_n = w->t_n < WEAR_TEMP_HIST ? w->t_n + 1 : WEAR_TEMP_HIST;
	int span = w->t_n - 1;
	float oldest = w->temp[(w->t_head + WEAR_TEMP_HIST - w->t_n) % WEAR_TEMP_HIST];

	return span > 0 ? (t - oldest) * 60.0f / span : 0.0f;
}

bool wear_update(struct wear *w, bool dc_valid, float dc_per_ma, bool pulse, float motion_mg,
		 bool temp_valid, float temp_c)
{
	const struct wear_cfg *c = &w->cfg;

	if (!dc_valid) {
		return w->worn; /* no green sample this second: hold */
	}
	/* no reading: neither cool nor below warm, so only darkness takes the pod off */
	float rate = temp_valid ? temp_rate(w, temp_c) : 0.0f;
	bool cool = temp_valid && (temp_c < c->cool_c || rate < c->cool_rate);
	bool not_warm = temp_valid && temp_c < c->warm_c;

	if (w->worn && pulse && w->learn_n < c->learn_s) {
		w->learn_sum += (double)dc_per_ma;
		if (++w->learn_n == c->learn_s) {
			w->dc_ref = (float)(w->learn_sum / c->learn_s);
		}
	}
	float dc_rel = w->dc_ref > 0 ? dc_per_ma / w->dc_ref : 0.0f;

	w->since_pulse = pulse ? 0 : inc(w->since_pulse);
	if (w->worn) {
		w->dark = dc_rel < c->dc_off ? inc(w->dark) : 0;
		bool still = motion_mg < c->still_mg;

		if (w->dark >= c->dark_s || (w->since_pulse >= c->no_pulse_s && still && cool) ||
		    (w->since_pulse >= c->no_pulse_long_s && not_warm)) {
			w->worn = false;
			w->lit = 0;
			w->pending = -1;
		}
	} else {
		w->lit = dc_rel > c->dc_on ? inc(w->lit) : 0;
		if (w->pending < 0 && w->lit >= c->on_s) {
			w->pending = 0;
		}
		if (w->pending >= 0) {
			if (pulse) {
				w->worn = true;
				w->dark = 0;
				w->since_pulse = 0;
				w->pending = -1;
			} else if (dc_rel < c->dc_on || w->pending >= c->confirm_s) {
				w->pending = -1;
			} else {
				w->pending++;
			}
		}
	}
	return w->worn;
}
