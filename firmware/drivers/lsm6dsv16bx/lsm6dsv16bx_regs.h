/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * LSM6DSV16BX (pod U11) registers and the register logic of the driver, in plain C so the host
 * tests pin it. Addresses and bit positions are from ST's lsm6dsv16bx-pid register driver
 * (lsm6dsv16bx_reg.h, github.com/STMicroelectronics/lsm6dsv16bx-pid) and data sheet DS13771.
 * The pod uses the accelerometer only: the gyroscope stays off (CTRL2 = 0), and TDM and Qvar
 * stay off because their pins are tied on the EVT board (TDMout, BCLK, AH1, AH2 to GND).
 */
#ifndef LSM6DSV16BX_REGS_H_
#define LSM6DSV16BX_REGS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LSM6DSV16BX_WHO_AM_I_VAL 0x71 /* the LSM6DSV16X reads 0x70, the LSM6DSOX 0x6C */

/* main page */
#define LSM6DSV16BX_REG_FUNC_CFG_ACCESS 0x01
#define LSM6DSV16BX_REG_PIN_CTRL        0x02
#define LSM6DSV16BX_REG_IF_CFG          0x03 /* 0 i2c_i3c_disable, 3 pp_od, 4 h_lactive, 6 tdm_out_pu_en */
#define LSM6DSV16BX_REG_FIFO_CTRL1      0x07 /* WTM[7:0] */
#define LSM6DSV16BX_REG_FIFO_CTRL2      0x08 /* 6 fifo_compr_rt_en, 7 stop_on_wtm */
#define LSM6DSV16BX_REG_FIFO_CTRL3      0x09 /* BDR_XL[3:0], BDR_GY[7:4] */
#define LSM6DSV16BX_REG_FIFO_CTRL4      0x0A /* FIFO_MODE[2:0]: 0 bypass, 1 FIFO (stops when full) */
#define LSM6DSV16BX_REG_INT1_CTRL       0x0D /* 0 INT1_DRDY_XL */
#define LSM6DSV16BX_REG_WHO_AM_I        0x0F
#define LSM6DSV16BX_REG_CTRL1           0x10 /* ODR_XL[3:0], OP_MODE_XL[6:4] */
#define LSM6DSV16BX_REG_CTRL2           0x11 /* gyroscope ODR: 0 = off */
#define LSM6DSV16BX_REG_CTRL3           0x12 /* 0 SW_RESET, 2 IF_INC, 6 BDU, 7 BOOT */
#define LSM6DSV16BX_REG_CTRL4           0x13 /* 1 DRDY_PULSED */
#define LSM6DSV16BX_REG_CTRL7           0x16 /* 7 AH_QVAR_EN: kept 0 */
#define LSM6DSV16BX_REG_CTRL8           0x17 /* FS_XL[1:0]: 0 = +-2 g */
#define LSM6DSV16BX_REG_CTRL9           0x18 /* 3 LPF2_XL_EN */
#define LSM6DSV16BX_REG_FIFO_STATUS1    0x1B /* DIFF_FIFO[7:0], then FIFO_STATUS2 */
#define LSM6DSV16BX_REG_STATUS          0x1E /* 0 XLDA */
#define LSM6DSV16BX_REG_OUTZ_L_A        0x28 /* Z, Y, X, little-endian: Z comes first */
#define LSM6DSV16BX_REG_FIFO_OUT        0x78 /* FIFO_DATA_OUT_TAG, then Z, Y, X */

#define LSM6DSV16BX_CTRL3_SW_RESET 0x01
#define LSM6DSV16BX_CTRL3_IF_INC   0x04
#define LSM6DSV16BX_CTRL3_BDU      0x40
#define LSM6DSV16BX_CTRL4_DRDY_PULSED 0x02
#define LSM6DSV16BX_INT1_DRDY_XL   0x01

/* OP_MODE_XL */
#define LSM6DSV16BX_OP_HP      0x0 /* high-performance */
#define LSM6DSV16BX_OP_LP_2AVG 0x4 /* low-power, 2 averages (up to 240 Hz) */

/* +-2 g: 0.061 mg per LSB (ST's lsm6dsv16bx_from_fs2_to_mg) */
#define LSM6DSV16BX_UG_PER_LSB 61

/* ODR_XL code for a rate in mHz, exact: 15000 -> 0x3, 120000 -> 0x6, 3840000 -> 0xB,
 * 7680000 -> 0xC; 0 for 0 (power-down). Returns -1 for a rate the chip does not have. */
int lsm6dsv16bx_odr_code(uint32_t mhz);

/* The rate of an ODR_XL code, mHz (0 for 0 or an unknown code). */
uint32_t lsm6dsv16bx_odr_mhz(uint8_t code);

/* CTRL1 for a rate: low-power 2-average up to lp_max_mhz, high-performance above; 0 for
 * power-down. Returns -1 for a rate the chip does not have. */
int lsm6dsv16bx_ctrl1(uint32_t mhz, uint32_t lp_max_mhz);

/* OUTZ_L_A..OUTX_H_A (or one FIFO word's 6 data bytes), in chip order x, y, z */
void lsm6dsv16bx_xyz(const uint8_t raw[6], int16_t xyz[3]);

/* One axis in micro m/s^2 at +-2 g */
int32_t lsm6dsv16bx_lsb_to_ums2(int16_t lsb);

#endif /* LSM6DSV16BX_REGS_H_ */
