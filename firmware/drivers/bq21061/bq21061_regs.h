/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * BQ21061 register map and the pod's charger settings (TI SLUSDU0A, section 7.5).
 * Plain C, no Zephyr, so the host tests cover it.
 */
#ifndef ATRIUM_BQ21061_REGS_H_
#define ATRIUM_BQ21061_REGS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BQ21061_ADDR 0x6B

#define BQ21061_STAT0          0x00
#define BQ21061_STAT1          0x01
#define BQ21061_FLAG3          0x06 /* read-clear: bit 6 WD_FAULT_FLAG */
#define BQ21061_VBAT_CTRL      0x12
#define BQ21061_ICHG_CTRL      0x13
#define BQ21061_CHARGERCTRL0   0x17
#define BQ21061_ICCTRL0        0x35
#define BQ21061_ICCTRL2        0x37
#define BQ21061_TS_FASTCHGCTRL 0x61
#define BQ21061_TS_COLD        0x62
#define BQ21061_TS_COOL        0x63
#define BQ21061_TS_WARM        0x64
#define BQ21061_TS_HOT         0x65
#define BQ21061_DEVICE_ID      0x6F

#define BQ21061_DEVICE_ID_VAL 0x3A

/* STAT0 */
#define BQ21061_STAT0_VIN_PGOOD   (1U << 0)
#define BQ21061_STAT0_THERMREG    (1U << 1)
#define BQ21061_STAT0_CHARGE_DONE (1U << 5)
#define BQ21061_STAT0_CHRG_CV     (1U << 6)
/* STAT1 */
#define BQ21061_STAT1_TS_HOT       (1U << 0)
#define BQ21061_STAT1_TS_WARM      (1U << 1)
#define BQ21061_STAT1_TS_COOL      (1U << 2)
#define BQ21061_STAT1_TS_COLD      (1U << 3)
#define BQ21061_STAT1_BAT_UVLO     (1U << 4)
#define BQ21061_STAT1_BAT_OCP      (1U << 5)
#define BQ21061_STAT1_VIN_OVP      (1U << 7)
/* ICCTRL0 */
#define BQ21061_ICCTRL0_EN_SHIP_MODE (1U << 7)
/* ICCTRL2 */
#define BQ21061_ICCTRL2_CHARGER_DISABLE (1U << 0)

/* ICHG = 1.25 mA x code with ICHARGE_RANGE = 0 (the reset value) */
#define BQ21061_ICHG_STEP_UA 1250U

/*
 * TS settings for TH5 (NCP03XH103 on TS, with the board's parallel 10 kOhm), from the
 * TS bias of 80 uA and the Murata table (firmware review, 2026-11-07):
 *  - cool band (0.6 to 10.3 C): 0.375 x ICHG, 5.6 mA at 15 mA, under 0.2C (6 mA);
 *    warm band: VBATREG - 150 mV (reset value);
 *  - warm 0x3F, about 40 C; hot 0x38, 45 C: no charging above 45 C (reset: 60 C).
 * Cold (0x7C, 0.6 C) and cool (0x6D, 10.3 C) keep their reset values.
 */
#define BQ21061_TS_FASTCHGCTRL_VAL 0x35
#define BQ21061_TS_WARM_VAL        0x3F
#define BQ21061_TS_HOT_VAL         0x38

struct bq21061_write {
	uint8_t reg;
	uint8_t val;
};

/* ICHG code for a current, rounded down, 1 to 255 (0 is not a valid setting). */
uint8_t bq21061_ichg_code(uint32_t ua);

/*
 * The writes that set the charger up, in order: ICHG and the TS thresholds. The I2C
 * watchdog (50 s, on by default) returns every register to reset when the host goes
 * quiet, so firmware writes these again at every service. Returns the count.
 */
size_t bq21061_config(uint32_t ichg_ua, struct bq21061_write *out, size_t max);

/* ICCTRL2 with CHARGER_DISABLE set or cleared, other bits kept. */
uint8_t bq21061_icctrl2(uint8_t now, bool disable);

struct bq21061_status {
	bool vin_good;
	bool done;
	bool cv;
	bool thermreg;
	bool ts_cold;  /* TS: charging suspended, cold */
	bool ts_cool;  /* TS: reduced current */
	bool ts_warm;  /* TS: reduced voltage */
	bool ts_hot;   /* TS: charging suspended, hot */
	bool fault;    /* VIN over-voltage or battery over-current */
};

void bq21061_decode(uint8_t stat0, uint8_t stat1, struct bq21061_status *out);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_BQ21061_REGS_H_ */
