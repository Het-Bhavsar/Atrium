/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Power, charge and thermal pins and the SAADC: everything the power state
 * machine reads and drives.
 */
#ifndef ATRIUM_POWER_HW_H_
#define ATRIUM_POWER_HW_H_

#include <stdbool.h>
#include <stdint.h>

#include "charge_policy.h" /* struct temp_reading */

int power_hw_init(void);

/* VBUS_SENSE at the AIN0 pin, mV. */
int power_hw_vbus_sense_mv(uint32_t *mv);

/* VBAT from VBAT_SENSE (AIN5) and the divider in the devicetree, mV. */
int power_hw_vbat_mv(uint16_t *mv);

/* The two NTCs in one NTC_PWR pulse: TH1 skin, TH4 top. */
struct ntc_set {
	struct temp_reading skin; /* NT1_SENSE, TH1 */
	struct temp_reading top;  /* NT3_SENSE, TH4 */
};
void power_hw_ntcs(struct ntc_set *out, uint32_t now_ms);

bool power_hw_chrg_n_low(void);

/* true: drive CHG_INH low (charger off). false: Hi-Z. */
int power_hw_chg_inhibit(bool inhibit);

/*
 * Enter ship mode: SHIP high and held. The LPB1010 cuts the cell after tSM
 * (30 to 70 ms), so this normally does not return. If the pod is still alive
 * after 500 ms (a charger appeared), SHIP is released and -EAGAIN returned.
 */
int power_hw_ship(void);

#endif /* ATRIUM_POWER_HW_H_ */
