/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sense.h"

uint16_t sense_vdd_mv(uint32_t vref_sum, uint8_t n)
{
	if (vref_sum == 0) {
		return 0;
	}
	uint32_t mv = (uint32_t)SENSE_VREFINT_MV * SENSE_FS * n / vref_sum;

	return mv > UINT16_MAX ? UINT16_MAX : (uint16_t)mv;
}

int32_t sense_pod_ua(uint32_t sum, uint32_t zero_sum, uint8_t n, uint16_t vdd_mv)
{
	int64_t d = (int64_t)sum - (int64_t)zero_sum;
	/* uA = d / (FS * n) * VDD / gain / R, with VDD in mV and R in mOhm */
	int64_t num = d * vdd_mv * 1000000LL;
	int64_t den = (int64_t)SENSE_FS * n * SENSE_GAIN * SENSE_SHUNT_MOHM;

	return (int32_t)(num / den);
}

bool sense_saturated(uint32_t sum, uint8_t n)
{
	return sum >= (uint32_t)SENSE_SAT_CODE * n;
}

/* codes of the NTC divider from -20 C to 80 C in 5 C steps: TH1 to GND, R20 100k up.
 * TH1 is the Murata NCP15WF104F03RC (100k, B 4250 K) [VERIFY B against Murata's R-T table] */
static const uint16_t ntc_code[] = {949, 923, 890, 851, 805, 754, 697, 636, 574, 512, 452,
				    396, 344, 297, 255, 219, 187, 160, 137, 117, 100};
#define NTC_T0_C10 (-200)
#define NTC_STEP   50
#define NTC_N      (sizeof(ntc_code) / sizeof(ntc_code[0]))

int16_t sense_ntc_c10(uint32_t sum, uint8_t n)
{
	uint32_t c = n ? sum / n : 0;

	if (c >= 1000 || c <= 40) {
		return SENSE_TEMP_NONE; /* open (pulled to the top) or shorted */
	}
	if (c >= ntc_code[0]) {
		return NTC_T0_C10;
	}
	for (unsigned i = 1; i < NTC_N; i++) {
		if (c >= ntc_code[i]) {
			uint32_t hi = ntc_code[i - 1], lo = ntc_code[i];
			int32_t t = NTC_T0_C10 + (int32_t)(i - 1) * NTC_STEP;

			return (int16_t)(t + (int32_t)((hi - c) * NTC_STEP / (hi - lo)));
		}
	}
	return NTC_T0_C10 + (int16_t)((NTC_N - 1) * NTC_STEP);
}

/* resting voltage -> %, generic LiPo [ASSUMED]: measure the KP-502030 to replace it */
static const uint16_t batt_mv[] = {3400, 3500, 3600, 3650, 3700, 3750, 3800, 3900, 4000, 4100, 4200};
static const uint8_t batt_pc[] = {0, 5, 10, 20, 30, 40, 50, 65, 80, 90, 100};

uint8_t sense_batt_pct(uint16_t mv)
{
	if (mv <= batt_mv[0]) {
		return 0;
	}
	for (unsigned i = 1; i < sizeof(batt_mv) / sizeof(batt_mv[0]); i++) {
		if (mv <= batt_mv[i]) {
			uint32_t span = batt_mv[i] - batt_mv[i - 1];

			return (uint8_t)(batt_pc[i - 1] +
					 (mv - batt_mv[i - 1]) * (batt_pc[i] - batt_pc[i - 1]) / span);
		}
	}
	return 100;
}
