/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lsm6dsv16bx_regs.h"

/* ODR_XL codes 0x1-0xC (lsm6dsv16bx_xl_data_rate_t) */
static const uint32_t odr_mhz[] = {
	0, 1875, 7500, 15000, 30000, 60000, 120000, 240000, 480000, 960000, 1920000, 3840000, 7680000,
};

#define N_ODR (sizeof(odr_mhz) / sizeof(odr_mhz[0]))
#define LP_MAX_ODR_MHZ 240000 /* low-power modes run at 1.875 to 240 Hz */

int lsm6dsv16bx_odr_code(uint32_t mhz)
{
	for (size_t i = 0; i < N_ODR; i++) {
		if (odr_mhz[i] == mhz) {
			return (int)i;
		}
	}
	return -1;
}

uint32_t lsm6dsv16bx_odr_mhz(uint8_t code)
{
	return code < N_ODR ? odr_mhz[code] : 0;
}

int lsm6dsv16bx_ctrl1(uint32_t mhz, uint32_t lp_max_mhz)
{
	int odr = lsm6dsv16bx_odr_code(mhz);

	if (odr <= 0) {
		return odr; /* 0: power-down; -1: no such rate */
	}
	bool lp = mhz <= lp_max_mhz && mhz <= LP_MAX_ODR_MHZ && mhz >= 1875;

	return odr | ((lp ? LSM6DSV16BX_OP_LP_2AVG : LSM6DSV16BX_OP_HP) << 4);
}

void lsm6dsv16bx_xyz(const uint8_t raw[6], int16_t xyz[3])
{
	xyz[2] = (int16_t)(raw[0] | (raw[1] << 8));
	xyz[1] = (int16_t)(raw[2] | (raw[3] << 8));
	xyz[0] = (int16_t)(raw[4] | (raw[5] << 8));
}

int32_t lsm6dsv16bx_lsb_to_ums2(int16_t lsb)
{
	/* lsb x 61 ug x 9.80665 m/s^2 per g */
	return (int32_t)((int64_t)lsb * LSM6DSV16BX_UG_PER_LSB * 980665 / 100000);
}
