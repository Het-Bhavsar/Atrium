/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Firmware charge temperature policy, a second line behind the charger's own TS function.
 * Plain C, no Zephyr dependencies.
 *
 * The cell NTC TH5 sits on the BQ21061 TS pin, which only the charger reads: it stops
 * charging below 0 C and above 45 C and cuts the current to 0.375 x ICHG from 0 to 10 C
 * (bq21061_regs.h). Firmware adds the same window on its own sensor:
 *   below 0 C or above 45 C   charging disabled (ICCTRL2 CHARGER_DISABLE)
 *   0 to 10 C                 ICHG set to the cold current, at most 0.2C
 *   10 to 45 C                full ICHG
 * Its primary sensor is the top NTC TH4, which sits next to the nRF and reads warmer than
 * the cell, so the hot stop errs early. An optional backup takes over when the primary is
 * stale (none on the EVT board). TH1 sits on the skin, so it is left out. With both fresh,
 * the primary decides, and a disagreement beyond max_disagree_cc is a fault.
 */
#ifndef ATRIUM_CHARGE_POLICY_H_
#define ATRIUM_CHARGE_POLICY_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Temperatures are in centi-degrees Celsius (0.01 C). */
struct charge_policy_cfg {
	int32_t t_min_cc;           /* below: no charging */
	int32_t t_cold_cc;          /* below: average limited to cold_rate */
	int32_t t_max_cc;           /* above: no charging */
	int32_t hyst_cc;            /* distance back inside a limit before a band is left */
	int32_t max_disagree_cc;    /* proxies further apart than this count as a fault */
	uint32_t temp_max_age_ms;   /* a reading older than this is not used */
};

#define CHARGE_POLICY_CFG_DEFAULT                                                 \
	{                                                                         \
		.t_min_cc = 0, .t_cold_cc = 1000, .t_max_cc = 4500, .hyst_cc = 100,  \
		.max_disagree_cc = 1000, .temp_max_age_ms = 10000,                 \
	}

enum charge_band {
	CHG_BAND_FAULT = 0, /* no fresh reading, or the proxies disagree */
	CHG_BAND_FREEZE,    /* below t_min */
	CHG_BAND_COLD,      /* t_min to t_cold */
	CHG_BAND_NORMAL,    /* t_cold to t_max */
	CHG_BAND_HOT,       /* above t_max */
};

struct temp_reading {
	bool valid;
	int32_t cc;
	uint32_t at_ms; /* when it was taken, same clock as now_ms */
};

struct charge_temps {
	struct temp_reading primary; /* TH4, top side */
	struct temp_reading backup;  /* optional; never valid on the EVT board */
};

struct charge_policy {
	struct charge_policy_cfg cfg;
	enum charge_band band;
	bool inhibit;
};

void charge_policy_init(struct charge_policy *p, const struct charge_policy_cfg *cfg);

/*
 * Run once per temperature reading, or at least once a second while docked.
 * Returns true when charging must be disabled. While undocked the band is still
 * tracked, but nothing is disabled (the charger has no input anyway).
 * In CHG_BAND_COLD charging continues at the cold current.
 */
bool charge_policy_update(struct charge_policy *p, uint32_t now_ms, bool docked,
			  const struct charge_temps *t);

const char *charge_band_name(enum charge_band band);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_CHARGE_POLICY_H_ */
