/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "bq21061_regs.h"

uint8_t bq21061_ichg_code(uint32_t ua)
{
	uint32_t c = ua / BQ21061_ICHG_STEP_UA;

	return (uint8_t)(c < 1 ? 1 : c > 255 ? 255 : c);
}

size_t bq21061_config(uint32_t ichg_ua, struct bq21061_write *out, size_t max)
{
	const struct bq21061_write w[] = {
		{BQ21061_ICHG_CTRL, bq21061_ichg_code(ichg_ua)},
		{BQ21061_TS_FASTCHGCTRL, BQ21061_TS_FASTCHGCTRL_VAL},
		/* warm (cooler) sits above hot in TS volts: 0x3F > 0x38 */
		{BQ21061_TS_WARM, BQ21061_TS_WARM_VAL},
		{BQ21061_TS_HOT, BQ21061_TS_HOT_VAL},
	};
	size_t n = sizeof(w) / sizeof(w[0]);

	if (max < n) {
		return 0;
	}
	for (size_t i = 0; i < n; i++) {
		out[i] = w[i];
	}
	return n;
}

uint8_t bq21061_icctrl2(uint8_t now, bool disable)
{
	return disable ? (uint8_t)(now | BQ21061_ICCTRL2_CHARGER_DISABLE)
		       : (uint8_t)(now & ~BQ21061_ICCTRL2_CHARGER_DISABLE);
}

void bq21061_decode(uint8_t stat0, uint8_t stat1, struct bq21061_status *out)
{
	out->vin_good = stat0 & BQ21061_STAT0_VIN_PGOOD;
	out->thermreg = stat0 & BQ21061_STAT0_THERMREG;
	out->done = stat0 & BQ21061_STAT0_CHARGE_DONE;
	out->cv = stat0 & BQ21061_STAT0_CHRG_CV;
	out->ts_hot = stat1 & BQ21061_STAT1_TS_HOT;
	out->ts_warm = stat1 & BQ21061_STAT1_TS_WARM;
	out->ts_cool = stat1 & BQ21061_STAT1_TS_COOL;
	out->ts_cold = stat1 & BQ21061_STAT1_TS_COLD;
	out->fault = stat1 & (BQ21061_STAT1_VIN_OVP | BQ21061_STAT1_BAT_OCP);
}
