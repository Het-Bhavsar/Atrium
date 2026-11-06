/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ntc.h"

#include <math.h>

/* Valid NTC range: about -40 C (195 kOhm) to 125 C (340 Ohm). */
#define NTC_R_MIN_OHM 300
#define NTC_R_MAX_OHM 200000

uint32_t ntc_resistance_ohm(int32_t code, int32_t full_scale)
{
	if (full_scale <= 0 || code <= 0 || code >= full_scale) {
		return 0;
	}
	uint64_t r = (uint64_t)NTC_R_FIXED_OHM * (uint32_t)code / (uint32_t)(full_scale - code);

	return r > UINT32_MAX ? 0 : (uint32_t)r;
}

int32_t ntc_temp_cc(int32_t code, int32_t full_scale)
{
	uint32_t r = ntc_resistance_ohm(code, full_scale);

	if (r < NTC_R_MIN_OHM || r > NTC_R_MAX_OHM) {
		return NTC_INVALID;
	}
	const float t0 = 298.15f;
	float inv_t = 1.0f / t0 + logf((float)r / (float)NTC_R25_OHM) / (float)NTC_BETA_K;
	float t_c = 1.0f / inv_t - 273.15f;

	return (int32_t)lroundf(t_c * 100.0f);
}
