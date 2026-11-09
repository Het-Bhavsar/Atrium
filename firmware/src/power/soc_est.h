/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * State of charge without a fuel gauge (the pod has none). Inputs: the cell voltage from
 * VSYS_SENSE (VSYS x 1 M / 3.2 M on P0.28/AIN4; VSYS is about VBAT off the Plinth), and the
 * pod's own current, known from the active mode's budget. It combines two estimates:
 *  - Coulomb count: every second, subtract the mode current; on the dock,
 *    add the charge current while the charger runs.
 *  - Voltage: open-circuit voltage = VBAT + I x R_cell, mapped through a
 *    generic LiCoO2 OCV curve [ASSUMED, the one in logic_sim.py]. This
 *    pulls the count back, slowly, and only at light load, where I x R
 *    error is small.
 * The BQ21061 reporting charge done means full.
 * Plain C, no Zephyr.
 */
#ifndef ATRIUM_SOC_EST_H_
#define ATRIUM_SOC_EST_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct soc_cfg {
	uint16_t cell_mah;
	uint32_t cell_mohm;     /* for the I x R open-circuit correction */
	uint32_t light_load_ua; /* voltage correction only at or below this load */
	uint16_t pull_permille; /* share of the voltage error removed per reading at light load */
};

#define SOC_CFG_DEFAULT                                                           \
	{                                                                         \
		.cell_mah = 30, .cell_mohm = 2000, .light_load_ua = 400,           \
		.pull_permille = 20,                                               \
	}

struct soc {
	struct soc_cfg cfg;
	int64_t q_uams; /* charge in the cell, uA x ms (1 mAh = 3.6e9) */
	bool primed;
};

/* SoC in 0.1 % from an open-circuit voltage, on the generic curve. */
uint16_t soc_from_ocv_mv(uint16_t ocv_mv);

void soc_init(struct soc *s, const struct soc_cfg *cfg);

/* A VBAT reading (mV, loaded) with the load it was taken at (uA). Primes the estimate on the
 * first call, then corrects it at light load. */
void soc_on_vbat(struct soc *s, uint16_t vbat_mv, uint32_t load_ua);

/* Charge flow over dt_ms: out_ua drawn by the pod, in_ua from the charger (0 off the dock). */
void soc_flow(struct soc *s, uint32_t dt_ms, uint32_t out_ua, uint32_t in_ua);

/* The charger finished its cycle: the cell is full. */
void soc_full(struct soc *s);

/* State of charge in % (0..100), or 0 before the first reading. */
uint8_t soc_percent(const struct soc *s);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_SOC_EST_H_ */
