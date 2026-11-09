/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * NTCs TH1 (skin) and TH4 (top): Murata NCP03XH103F05RL, 10 kOhm 1 % at 25 C, each under
 * a 10 kOhm 0.1 % resistor from NTC_EN (P0.17). The SAADC reads them against VDD/4 at
 * gain 1/4, so the code is ratiometric and independent of VDD. Temperature comes from
 * Murata's own resistance-temperature table (centre values, NC-R8021), 1 C steps from
 * -40 to 85 C, interpolated linearly. Plain C, no Zephyr.
 */
#ifndef ATRIUM_NTC_H_
#define ATRIUM_NTC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NTC_R_FIXED_OHM 10000
#define NTC_T_MIN_C     (-40)
#define NTC_T_MAX_C     85

/* Returned for an open or shorted NTC, or a reading outside -40..85 C. */
#define NTC_INVALID INT32_MIN

/* NTC resistance from a ratiometric code (0..full_scale). 0 when out of range. */
uint32_t ntc_resistance_ohm(int32_t code, int32_t full_scale);

/* Temperature in centi-degrees C for a resistance, or NTC_INVALID. */
int32_t ntc_temp_cc_of_ohm(uint32_t r);

/* Temperature in centi-degrees C for a code, or NTC_INVALID. */
int32_t ntc_temp_cc(int32_t code, int32_t full_scale);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_NTC_H_ */
