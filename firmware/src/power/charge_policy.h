/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Charge temperature policy. Decides when firmware holds CHG_INH low to stop
 * the BQ25101 (through R16 to TS). Plain C, no Zephyr dependencies.
 *
 * Rules (atrium-rev-b1 SCHEMATIC_SIM.md 5.4, logic_sim.py FW_CHG_WINDOW / FW_COLD):
 *   below 0 C or above 45 C   no charging
 *   0 to 10 C                 duty-cycle CHG_INH so the average is <= 0.2C
 *   10 to 45 C                the charger runs on its own
 * The cell NTC TH2 sits on TS, which only the charger reads (and which stops
 * charging on its own at its hard limits). Firmware sees the cell through a
 * primary proxy, the top NTC TH4, and an optional backup (none on the r5 board, which
 * has no TH3). TH1 sits on the skin, so it is left out. With both fresh, the
 * primary decides, and a disagreement beyond max_disagree_cc is a fault, so a
 * broken primary cannot hide a hot cell. With one fresh, that one decides.
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
	uint16_t cell_mah;          /* rated capacity the C-rate is based on */
	uint16_t cold_rate_milli_c; /* 200 = 0.2C */
	uint32_t chg_cold_ua;       /* worst-case charger current in the cold band */
	uint32_t period_ms;         /* duty-cycle period in the cold band */
	uint32_t update_ms;         /* longest gap between updates: the on phase ends this early */
	uint32_t temp_max_age_ms;   /* a reading older than this is not used */
};

/* Defaults for Rev B.2: 30 mAh cell, R1 = 13.3 kOhm, half current 10.90 / 2 mA. The
 * firmware overrides the cell and current from the board devicetree. */
#define CHARGE_POLICY_CFG_DEFAULT                                                 \
	{                                                                         \
		.t_min_cc = 0, .t_cold_cc = 1000, .t_max_cc = 4500, .hyst_cc = 100,  \
		.max_disagree_cc = 1000, .cell_mah = 30, .cold_rate_milli_c = 200,  \
		.chg_cold_ua = 5451, .period_ms = 60000, .update_ms = 1000,          \
		.temp_max_age_ms = 10000,                                          \
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
	struct temp_reading primary; /* TH4, top side, nearest the cell */
	struct temp_reading backup;  /* optional; never valid on r5 (no TH3) */
};

struct charge_policy {
	struct charge_policy_cfg cfg;
	enum charge_band band;
	uint32_t cold_start_ms;
	bool inhibit;
};

void charge_policy_init(struct charge_policy *p, const struct charge_policy_cfg *cfg);

/*
 * Run once per temperature reading, or at least once a second while docked.
 * Returns true when CHG_INH must be driven low. While undocked the band is
 * still tracked, but the pin is released (the charger is unpowered anyway).
 */
bool charge_policy_update(struct charge_policy *p, uint32_t now_ms, bool docked,
			  const struct charge_temps *t);

/*
 * On-time per period in the cold band, so that chg_cold_ua * on / period <= cold limit.
 * The pin only changes on an update, so the policy ends the on phase update_ms early.
 */
uint32_t charge_policy_cold_on_ms(const struct charge_policy_cfg *cfg);

const char *charge_band_name(enum charge_band band);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_CHARGE_POLICY_H_ */
