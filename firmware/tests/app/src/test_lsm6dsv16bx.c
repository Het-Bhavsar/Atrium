/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The LSM6DSV16BX driver's register logic (drivers/lsm6dsv16bx/lsm6dsv16bx_regs.c) against ST's
 * register driver: ODR codes, power mode, axis order and scale. And every rate a mode asks
 * for is one the chip has, so no mode change fails at run time.
 */
#include <zephyr/ztest.h>

#include "lsm6dsv16bx_regs.h"
#include "modes.h"

ZTEST_SUITE(lsm6dsv16bx, NULL, NULL, NULL, NULL, NULL);

ZTEST(lsm6dsv16bx, test_odr_codes)
{
	/* lsm6dsv16bx_xl_data_rate_t */
	zassert_equal(lsm6dsv16bx_odr_code(0), 0);
	zassert_equal(lsm6dsv16bx_odr_code(1875), 0x1);
	zassert_equal(lsm6dsv16bx_odr_code(15000), 0x3);
	zassert_equal(lsm6dsv16bx_odr_code(120000), 0x6);
	zassert_equal(lsm6dsv16bx_odr_code(3840000), 0xB);
	zassert_equal(lsm6dsv16bx_odr_code(7680000), 0xC);
	/* the LSM6DSOX's rates are not this chip's */
	zassert_equal(lsm6dsv16bx_odr_code(12500), -1);
	zassert_equal(lsm6dsv16bx_odr_code(104000), -1);
	zassert_equal(lsm6dsv16bx_odr_code(3333333), -1);
	for (uint8_t c = 0; c <= 0xC; c++) {
		zassert_equal(lsm6dsv16bx_odr_code(lsm6dsv16bx_odr_mhz(c)), c, "code %u", c);
	}
	zassert_equal(lsm6dsv16bx_odr_mhz(0xD), 0);
}

ZTEST(lsm6dsv16bx, test_ctrl1_power_mode)
{
	/* OP_MODE_XL [6:4]: 100 low-power 2 averages up to the limit, 000 high-performance */
	zassert_equal(lsm6dsv16bx_ctrl1(0, 15000), 0x00, "power-down");
	zassert_equal(lsm6dsv16bx_ctrl1(15000, 15000), 0x43);
	zassert_equal(lsm6dsv16bx_ctrl1(120000, 15000), 0x06);
	zassert_equal(lsm6dsv16bx_ctrl1(7680000, 15000), 0x0C);
	/* low-power stops at 240 Hz whatever the limit */
	zassert_equal(lsm6dsv16bx_ctrl1(240000, 10000000), 0x47);
	zassert_equal(lsm6dsv16bx_ctrl1(480000, 10000000), 0x08);
	zassert_equal(lsm6dsv16bx_ctrl1(12500, 15000), -1);
}

ZTEST(lsm6dsv16bx, test_axes_z_first)
{
	/* OUTZ_L_A at 0x28: z, y, x */
	const uint8_t raw[6] = {0x03, 0x00, 0xFE, 0xFF, 0x00, 0x40};
	int16_t xyz[3];

	lsm6dsv16bx_xyz(raw, xyz);
	zassert_equal(xyz[0], 0x4000, "x last");
	zassert_equal(xyz[1], -2);
	zassert_equal(xyz[2], 3, "z first");
}

ZTEST(lsm6dsv16bx, test_scale_2g)
{
	/* 0.061 mg per LSB: 16384 LSB = 999.4 mg = 9.8010 m/s^2 */
	zassert_within(lsm6dsv16bx_lsb_to_ums2(16384), 9801001, 2);
	zassert_within(lsm6dsv16bx_lsb_to_ums2(-32768), -19602002, 2);
	zassert_equal(lsm6dsv16bx_lsb_to_ums2(0), 0);
}

ZTEST(lsm6dsv16bx, test_every_mode_rate_exists)
{
	for (int m = 0; m < MODE_COUNT; m++) {
		uint32_t mhz = mode_get(m)->imu_mhz;

		zassert_true(lsm6dsv16bx_ctrl1(mhz, 15000) >= 0, "%s: %u mHz", mode_get(m)->name, mhz);
	}
	zassert_equal(MODE_ALGO_IMU_MHZ, 15000);
	zassert_equal(lsm6dsv16bx_odr_code(MODE_ALGO_IMU_MHZ), 0x3);
}
