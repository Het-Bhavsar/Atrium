/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ntc.h"

/* Murata NCP03XH103F05RL, "Resistance VS. Temperature (Reference Values)" NC-R8021, centre
 * column in ohms, -40 to 85 C in 1 C steps (pim.murata.com) */
static const uint32_t rt_ohm[NTC_T_MAX_C - NTC_T_MIN_C + 1] = {
	195652, 184917, 174845, 165391, 156512, 148171, 140330, 132958, /* -40 C */
	126022, 119494, 113347, 107565, 102116, 96978, 92132, 87559, /* -32 C */
	83242, 79166, 75316, 71677, 68237, 64991, 61919, 59011, /* -24 C */
	56258, 53650, 51178, 48835, 46613, 44506, 42506, 40600, /* -16 C */
	38790, 37073, 35442, 33892, 32420, 31020, 29689, 28423, /* -8 C */
	27219, 26076, 24988, 23951, 22963, 22021, 21123, 20267, /* 0 C */
	19450, 18670, 17926, 17214, 16534, 15886, 15266, 14674, /* 8 C */
	14108, 13566, 13049, 12554, 12080, 11628, 11195, 10780, /* 16 C */
	10382, 10000, 9634, 9284, 8947, 8624, 8314, 8018, /* 24 C */
	7734, 7461, 7199, 6948, 6707, 6475, 6253, 6039, /* 32 C */
	5834, 5636, 5445, 5262, 5086, 4917, 4754, 4597, /* 40 C */
	4446, 4301, 4161, 4026, 3896, 3771, 3651, 3535, /* 48 C */
	3423, 3315, 3211, 3111, 3014, 2922, 2834, 2748, /* 56 C */
	2666, 2586, 2509, 2435, 2364, 2294, 2228, 2163, /* 64 C */
	2100, 2040, 1981, 1924, 1870, 1817, 1766, 1716, /* 72 C */
	1668, 1622, 1578, 1534, 1493, 1452, /* 80 C */
};
#define N_RT (sizeof(rt_ohm) / sizeof(rt_ohm[0]))

uint32_t ntc_resistance_ohm(int32_t code, int32_t full_scale)
{
	if (full_scale <= 0 || code <= 0 || code >= full_scale) {
		return 0;
	}
	uint64_t r = (uint64_t)NTC_R_FIXED_OHM * (uint32_t)code / (uint32_t)(full_scale - code);

	return r > UINT32_MAX ? 0 : (uint32_t)r;
}

int32_t ntc_temp_cc_of_ohm(uint32_t r)
{
	if (r > rt_ohm[0] || r < rt_ohm[N_RT - 1]) {
		return NTC_INVALID;
	}
	/* the table falls with temperature: find rt_ohm[i] >= r > rt_ohm[i + 1] */
	unsigned int lo = 0, hi = N_RT - 1;

	while (hi - lo > 1) {
		unsigned int mid = (lo + hi) / 2;

		if (rt_ohm[mid] >= r) {
			lo = mid;
		} else {
			hi = mid;
		}
	}
	uint32_t span = rt_ohm[lo] - rt_ohm[hi];
	int32_t frac_cc = (int32_t)(((uint64_t)(rt_ohm[lo] - r) * 100U + span / 2U) / span);

	return (NTC_T_MIN_C + (int32_t)lo) * 100 + frac_cc;
}

int32_t ntc_temp_cc(int32_t code, int32_t full_scale)
{
	uint32_t r = ntc_resistance_ohm(code, full_scale);

	return r ? ntc_temp_cc_of_ohm(r) : NTC_INVALID;
}
