/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Thermal and supply sensing on the SAADC, and System OFF.
 */
#ifndef ATRIUM_POWER_HW_H_
#define ATRIUM_POWER_HW_H_

#include <stdbool.h>
#include <stdint.h>

#include "charge_policy.h" /* struct temp_reading */

int power_hw_init(void);

/* VSYS from VSYS_SENSE (AIN4) and the divider in the devicetree, mV. */
int power_hw_vsys_mv(uint16_t *mv);

/* The two NTCs in one NTC_EN pulse: TH1 skin, TH4 top. */
struct ntc_set {
	struct temp_reading skin; /* TH1 */
	struct temp_reading top;  /* TH4 */
};
void power_hw_ntcs(struct ntc_set *out, uint32_t now_ms);

/*
 * System OFF: the caller has already shut the sensors down. The nRF wakes, through a
 * reset, when /PG goes low (pod placed on the Plinth). Does not return.
 */
void power_hw_poweroff(void);

#endif /* ATRIUM_POWER_HW_H_ */
