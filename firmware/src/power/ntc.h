/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Edge NTC TH1 (NCP03XH103F05RL, 10 kOhm at 25 C, B25/50 3380 K) under
 * R13 10 kOhm 0.1 % from NTC_PWR (P0.04) to NTC_SENSE (P0.03/AIN1).
 * The SAADC reads it against VDD/4 at gain 1/4, so the code is ratiometric
 * and independent of the 1.8 V rail. Beta model [VERIFY: against the maker's
 * R-T table and the bench, SCHEMATIC_SIM.md 2 quotes +-0.55 K uncalibrated].
 */
#ifndef ATRIUM_NTC_H_
#define ATRIUM_NTC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NTC_R_FIXED_OHM 10000
#define NTC_R25_OHM     10000
#define NTC_BETA_K      3380

/* Returned for an open or shorted NTC. */
#define NTC_INVALID INT32_MIN

/* NTC resistance from a ratiometric code (0..full_scale). 0 when out of range. */
uint32_t ntc_resistance_ohm(int32_t code, int32_t full_scale);

/* Temperature in centi-degrees C, or NTC_INVALID. */
int32_t ntc_temp_cc(int32_t code, int32_t full_scale);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_NTC_H_ */
