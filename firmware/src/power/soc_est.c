/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "soc_est.h"

/* Generic LiCoO2 pouch OCV against SoC (logic_sim.py SOC_T / OCV_T) [ASSUMED] */
static const uint16_t soc_t[] = {0, 30, 60, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
static const uint16_t ocv_t[] = {3000, 3350, 3500, 3600, 3690, 3740, 3780,
				 3820, 3870, 3940, 4020, 4100, 4190};
#define N_PTS (sizeof(soc_t) / sizeof(soc_t[0]))

uint16_t soc_from_ocv_mv(uint16_t ocv_mv)
{
	if (ocv_mv <= ocv_t[0]) {
		return 0;
	}
	for (unsigned int i = 1; i < N_PTS; i++) {
		if (ocv_mv <= ocv_t[i]) {
			uint32_t span = ocv_t[i] - ocv_t[i - 1];

			return (uint16_t)(soc_t[i - 1] +
					  (uint32_t)(soc_t[i] - soc_t[i - 1]) * (ocv_mv - ocv_t[i - 1]) / span);
		}
	}
	return 1000;
}

static int64_t full_q(const struct soc *s)
{
	return (int64_t)s->cfg.cell_mah * 3600000000LL;
}

void soc_init(struct soc *s, const struct soc_cfg *cfg)
{
	s->cfg = *cfg;
	s->q_uams = 0;
	s->primed = false;
}

void soc_on_vbat(struct soc *s, uint16_t vbat_mv, uint32_t load_ua)
{
	uint32_t ocv = vbat_mv + (uint32_t)((uint64_t)load_ua * s->cfg.cell_mohm / 1000000U);
	int64_t from_v = full_q(s) * soc_from_ocv_mv(ocv > UINT16_MAX ? UINT16_MAX : (uint16_t)ocv) / 1000;

	if (!s->primed) {
		s->q_uams = from_v;
		s->primed = true;
		return;
	}
	if (load_ua <= s->cfg.light_load_ua) {
		s->q_uams += (from_v - s->q_uams) * s->cfg.pull_permille / 1000;
	}
}

void soc_flow(struct soc *s, uint32_t dt_ms, uint32_t out_ua, uint32_t in_ua)
{
	s->q_uams += ((int64_t)in_ua - (int64_t)out_ua) * dt_ms;
	if (s->q_uams < 0) {
		s->q_uams = 0;
	} else if (s->q_uams > full_q(s)) {
		s->q_uams = full_q(s);
	}
}

void soc_full(struct soc *s)
{
	s->q_uams = full_q(s);
	s->primed = true;
}

uint8_t soc_percent(const struct soc *s)
{
	if (!s->primed || s->cfg.cell_mah == 0) {
		return 0;
	}
	int64_t p = (s->q_uams * 100 + full_q(s) / 2) / full_q(s);

	return (uint8_t)(p > 100 ? 100 : p < 0 ? 0 : p);
}
