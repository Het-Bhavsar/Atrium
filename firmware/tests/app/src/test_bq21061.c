/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * BQ21061 settings against TI SLUSDU0A section 7.5 and the firmware review (2026-11-07).
 */
#include <zephyr/ztest.h>

#include "bq21061_regs.h"

ZTEST_SUITE(bq21061, NULL, NULL, NULL, NULL, NULL);

ZTEST(bq21061, test_ichg_codes)
{
	zassert_equal(bq21061_ichg_code(15000), 0x0C, "15 mA = 12 x 1.25 mA");
	zassert_equal(bq21061_ichg_code(5000), 0x04);
	zassert_equal(bq21061_ichg_code(6249), 0x04, "rounded down");
	zassert_equal(bq21061_ichg_code(0), 1, "0 is not a setting");
	zassert_equal(bq21061_ichg_code(1000000), 255);
}

ZTEST(bq21061, test_config_writes)
{
	struct bq21061_write w[4];

	zassert_equal(bq21061_config(15000, w, 3), 0, "too small a buffer");
	zassert_equal(bq21061_config(15000, w, ARRAY_SIZE(w)), 4);
	zassert_equal(w[0].reg, BQ21061_ICHG_CTRL);
	zassert_equal(w[0].val, 0x0C);
	/* cool band: TS_ICHRG 0b101 = 0.375 x 15 mA = 5.6 mA <= 0.2C of 30 mAh; warm band
	 * TS_VBAT_REG 0b011 = VBATREG - 150 mV, the reset value */
	zassert_equal(w[1].reg, BQ21061_TS_FASTCHGCTRL);
	zassert_equal(w[1].val & 0x07, 0x05);
	zassert_equal((w[1].val >> 4) & 0x07, 0x03);
	zassert_true(15000 * 375 / 1000 <= 30 * 200, "cool-band current within 0.2C");
	/* no charging above 45 C: TS_HOT 0x38 (262.5 mV), warm about 40 C above it in volts */
	zassert_equal(w[2].reg, BQ21061_TS_WARM);
	zassert_equal(w[3].reg, BQ21061_TS_HOT);
	zassert_equal(w[3].val, 0x38);
	zassert_true(w[2].val > w[3].val);
}

ZTEST(bq21061, test_charger_disable_keeps_other_bits)
{
	/* ICCTRL2 reset 0x40: PMID 4.5 V */
	zassert_equal(bq21061_icctrl2(0x40, true), 0x41);
	zassert_equal(bq21061_icctrl2(0x41, false), 0x40);
}

ZTEST(bq21061, test_status_decode)
{
	struct bq21061_status s;

	bq21061_decode(0x21, 0x04, &s);
	zassert_true(s.vin_good && s.done && s.ts_cool);
	zassert_false(s.cv || s.ts_cold || s.ts_hot || s.fault);
	bq21061_decode(0x41, 0x89, &s);
	zassert_true(s.cv && s.ts_hot && s.ts_cold && s.fault, "VIN over-voltage is a fault");
	bq21061_decode(0x01, 0x10, &s);
	zassert_false(s.fault, "battery under-voltage is pre-charge, not a fault");
}
