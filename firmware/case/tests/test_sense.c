/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>

#include "check.h"
#include "sense.h"

#define N 16

/* the ADC code for `mv` at VDD `vdd`, summed over N */
static uint32_t code_sum(double mv, double vdd)
{
	return (uint32_t)lround(mv / vdd * SENSE_FS * N);
}

static void vdd_from_vrefint(void)
{
	for (int vdd = 3300; vdd <= 4300; vdd += 100) {
		CHECK_NEAR(sense_vdd_mv(code_sum(SENSE_VREFINT_MV, vdd), N), vdd, 3);
	}
	CHECK_EQ(sense_vdd_mv(0, N), 0);
}

static void pod_current_after_zero(void)
{
	/* pedestal and offset: 0.5 V at the OPA output with no current, at any VDD */
	for (int vdd = 3400; vdd <= 4200; vdd += 400) {
		uint32_t zero = code_sum(500, vdd);
		int32_t ua_list[] = {0, 400, 2500, 5000, 15000, 80000};

		for (unsigned i = 0; i < sizeof(ua_list) / sizeof(ua_list[0]); i++) {
			double out_mv = 500 + ua_list[i] / 1000.0 * SENSE_SHUNT_MOHM / 1000.0 * SENSE_GAIN;
			int32_t got = sense_pod_ua(code_sum(out_mv, vdd), zero, N, (uint16_t)vdd);

			/* half an LSB of the 16-sample sum, in uA */
			double lsb_ua = vdd / (double)(SENSE_FS * N) / SENSE_GAIN * 1000.0;

			CHECK_NEAR(got, ua_list[i], lsb_ua + 1);
		}
	}
	CHECK(sense_pod_ua(100, 200, N, 4000) < 0);
}

static void saturation(void)
{
	CHECK(!sense_saturated(1014u * N, N));
	CHECK(sense_saturated(1015u * N, N));
	CHECK(sense_saturated(1023u * N, N));
}

static double ntc_c(uint32_t code)
{
	double r = 100e3 * code / (SENSE_FS - code);

	return 1.0 / (1.0 / 298.15 + log(r / 100e3) / 4250.0) - 273.15;
}

static void ntc_matches_beta(void)
{
	for (uint32_t c = 120; c <= 930; c += 7) {
		int16_t t = sense_ntc_c10(c * N, N);

		CHECK_NEAR(t / 10.0, ntc_c(c), 0.6);
	}
	CHECK_NEAR(sense_ntc_c10(512 * N, N), 250, 2);
	CHECK_NEAR(sense_ntc_c10(297 * N, N), 450, 2); /* the 45 C cut */
	CHECK_EQ(sense_ntc_c10(1010 * N, N), SENSE_TEMP_NONE); /* open */
	CHECK_EQ(sense_ntc_c10(20 * N, N), SENSE_TEMP_NONE);   /* shorted */
}

static void battery_percent(void)
{
	CHECK_EQ(sense_batt_pct(3300), 0);
	CHECK_EQ(sense_batt_pct(3400), 0);
	CHECK_EQ(sense_batt_pct(3650), 20);
	CHECK_EQ(sense_batt_pct(3800), 50);
	CHECK_EQ(sense_batt_pct(3900), 65);
	CHECK_EQ(sense_batt_pct(4200), 100);
	CHECK_EQ(sense_batt_pct(4300), 100);
	uint8_t last = 0;

	for (uint16_t mv = 3300; mv <= 4300; mv += 10) {
		uint8_t p = sense_batt_pct(mv);

		CHECK(p >= last);
		last = p;
	}
}

const struct check_case sense_cases[] = {
	{"sense: VDD from VREFINT", vdd_from_vrefint},
	{"sense: pod current after the zero", pod_current_after_zero},
	{"sense: clipped OPA output", saturation},
	{"sense: NTC table matches B 4250 K", ntc_matches_beta},
	{"sense: battery percent", battery_percent},
	CASES_END,
};
