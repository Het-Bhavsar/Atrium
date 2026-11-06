/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "charge_policy.h"

#include <stddef.h>

void charge_policy_init(struct charge_policy *p, const struct charge_policy_cfg *cfg)
{
	p->cfg = *cfg;
	p->band = CHG_BAND_FAULT;
	p->cold_start_ms = 0;
	p->inhibit = false;
}

uint32_t charge_policy_cold_on_ms(const struct charge_policy_cfg *cfg)
{
	/* cold limit in uA: mAh * milli-C, e.g. 40 * 200 = 8000 uA */
	uint64_t limit_ua = (uint64_t)cfg->cell_mah * cfg->cold_rate_milli_c;

	if (cfg->chg_cold_ua == 0 || limit_ua >= cfg->chg_cold_ua) {
		return cfg->period_ms;
	}
	/* round down so the average never exceeds the limit */
	return (uint32_t)(((uint64_t)cfg->period_ms * limit_ua) / cfg->chg_cold_ua);
}

static bool fresh(const struct temp_reading *r, uint32_t now_ms, uint32_t max_age_ms)
{
	return r->valid && (uint32_t)(now_ms - r->at_ms) <= max_age_ms;
}

static enum charge_band classify(const struct charge_policy *p, uint32_t now_ms,
				 const struct charge_temps *t)
{
	const struct charge_policy_cfg *c = &p->cfg;
	bool a = fresh(&t->primary, now_ms, c->temp_max_age_ms);
	bool b = fresh(&t->backup, now_ms, c->temp_max_age_ms);
	int32_t temp;

	if (a && b) {
		int32_t d = t->primary.cc - t->backup.cc;

		if (d > c->max_disagree_cc || -d > c->max_disagree_cc) {
			return CHG_BAND_FAULT;
		}
		temp = t->primary.cc;
	} else if (a) {
		temp = t->primary.cc;
	} else if (b) {
		temp = t->backup.cc;
	} else {
		return CHG_BAND_FAULT;
	}
	int32_t hi = temp, lo = temp;

	/* Each limit moves back by hyst_cc while its band is active. */
	int32_t t_max = c->t_max_cc - (p->band == CHG_BAND_HOT ? c->hyst_cc : 0);
	int32_t t_min = c->t_min_cc + (p->band == CHG_BAND_FREEZE ? c->hyst_cc : 0);
	bool in_cold = p->band == CHG_BAND_COLD || p->band == CHG_BAND_FREEZE;
	int32_t t_cold = c->t_cold_cc + (in_cold ? c->hyst_cc : 0);

	if (hi > t_max) {
		return CHG_BAND_HOT;
	}
	if (lo < t_min) {
		return CHG_BAND_FREEZE;
	}
	if (lo < t_cold) {
		return CHG_BAND_COLD;
	}
	return CHG_BAND_NORMAL;
}

bool charge_policy_update(struct charge_policy *p, uint32_t now_ms, bool docked,
			  const struct charge_temps *t)
{
	enum charge_band band = classify(p, now_ms, t);

	if (band == CHG_BAND_COLD && p->band != CHG_BAND_COLD) {
		p->cold_start_ms = now_ms; /* each cold stretch starts with an on phase */
	}
	p->band = band;

	if (!docked) {
		p->inhibit = false;
		return false;
	}

	switch (band) {
	case CHG_BAND_NORMAL:
		p->inhibit = false;
		break;
	case CHG_BAND_COLD: {
		if (p->cfg.period_ms == 0) {
			p->inhibit = true;
			break;
		}
		uint32_t on_ms = charge_policy_cold_on_ms(&p->cfg);
		uint32_t phase = (uint32_t)(now_ms - p->cold_start_ms) % p->cfg.period_ms;

		p->inhibit = phase + p->cfg.update_ms > on_ms;
		break;
	}
	default:
		p->inhibit = true;
		break;
	}
	return p->inhibit;
}

const char *charge_band_name(enum charge_band band)
{
	static const char *const names[] = {"fault", "freeze", "cold", "normal", "hot"};

	return (unsigned int)band < sizeof(names) / sizeof(names[0]) ? names[band] : "?";
}
