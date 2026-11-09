/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "charge_policy.h"

#include <stddef.h>

void charge_policy_init(struct charge_policy *p, const struct charge_policy_cfg *cfg)
{
	p->cfg = *cfg;
	p->band = CHG_BAND_FAULT;
	p->inhibit = false;
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

	p->band = band;
	p->inhibit = docked && band != CHG_BAND_NORMAL && band != CHG_BAND_COLD;
	return p->inhibit;
}

const char *charge_band_name(enum charge_band band)
{
	static const char *const names[] = {"fault", "freeze", "cold", "normal", "hot"};

	return (unsigned int)band < sizeof(names) / sizeof(names[0]) ? names[band] : "?";
}
