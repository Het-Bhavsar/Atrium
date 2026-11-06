/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "power_hw.h"

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include "ntc.h"

LOG_MODULE_REGISTER(power_hw, CONFIG_ATRIUM_LOG_LEVEL);

#define USER DT_PATH(zephyr_user)

static const struct adc_dt_spec adc_vbus = ADC_DT_SPEC_GET_BY_NAME(USER, vbus_sense);
static const struct adc_dt_spec adc_vbat = ADC_DT_SPEC_GET_BY_NAME(USER, vbat_sense);
static const struct adc_dt_spec adc_nt1 = ADC_DT_SPEC_GET_BY_NAME(USER, nt1_sense);
static const struct adc_dt_spec adc_nt3 = ADC_DT_SPEC_GET_BY_NAME(USER, nt3_sense);
static const struct gpio_dt_spec ship = GPIO_DT_SPEC_GET(USER, ship_gpios);
static const struct gpio_dt_spec chg_inh = GPIO_DT_SPEC_GET(USER, chg_inh_gpios);
static const struct gpio_dt_spec chrg_n = GPIO_DT_SPEC_GET(USER, chrg_n_gpios);
static const struct gpio_dt_spec ntc_pwr = GPIO_DT_SPEC_GET(USER, ntc_pwr_gpios);

#define VBAT_TOP_OHM DT_PROP(USER, vbat_sense_top_ohm)
#define VBAT_BOT_OHM DT_PROP(USER, vbat_sense_bottom_ohm)

/* SHIP hold before giving up: well past the LPB1010 tSM maximum of 70 ms */
#define SHIP_HOLD_MS 500

static int read_adc(const struct adc_dt_spec *spec, int32_t *raw)
{
	int16_t buf;
	struct adc_sequence seq = {.buffer = &buf, .buffer_size = sizeof(buf)};
	int ret = adc_sequence_init_dt(spec, &seq);

	ret = ret ? ret : adc_read_dt(spec, &seq);
	if (ret == 0) {
		*raw = buf;
	}
	return ret;
}

static int read_mv(const struct adc_dt_spec *spec, int32_t *mv)
{
	int ret = read_adc(spec, mv);

	return ret ? ret : adc_raw_to_millivolts_dt(spec, mv);
}

int power_hw_init(void)
{
	const struct adc_dt_spec *ch[] = {&adc_vbus, &adc_vbat, &adc_nt1, &adc_nt3};
	int ret = 0;

	if (!adc_is_ready_dt(&adc_vbus) || !gpio_is_ready_dt(&ship)) {
		return -ENODEV;
	}
	for (size_t i = 0; i < ARRAY_SIZE(ch) && !ret; i++) {
		ret = adc_channel_setup_dt(ch[i]);
	}
	/* SHIP low (the LPB1010 also pulls CTL down), CHG_INH Hi-Z, NTCs unpowered */
	ret = ret ? ret : gpio_pin_configure_dt(&ship, GPIO_OUTPUT_INACTIVE);
	ret = ret ? ret : gpio_pin_configure_dt(&chg_inh, GPIO_DISCONNECTED);
	ret = ret ? ret : gpio_pin_configure_dt(&chrg_n, GPIO_INPUT);
	ret = ret ? ret : gpio_pin_configure_dt(&ntc_pwr, GPIO_OUTPUT_INACTIVE);
	return ret;
}

int power_hw_vbus_sense_mv(uint32_t *mv)
{
	int32_t v;
	int ret = read_mv(&adc_vbus, &v);

	if (ret == 0) {
		*mv = v < 0 ? 0 : (uint32_t)v;
	}
	return ret;
}

int power_hw_vbat_mv(uint16_t *mv)
{
	int32_t v;
	int ret = read_mv(&adc_vbat, &v);

	if (ret == 0) {
		int64_t vbat = (int64_t)(v < 0 ? 0 : v) * (VBAT_TOP_OHM + VBAT_BOT_OHM) / VBAT_BOT_OHM;

		*mv = vbat > UINT16_MAX ? UINT16_MAX : (uint16_t)vbat;
	}
	return ret;
}

static void ntc_one(const struct adc_dt_spec *spec, struct temp_reading *r, uint32_t now)
{
	int32_t raw;

	r->valid = false;
	if (read_adc(spec, &raw) == 0) {
		int32_t cc = ntc_temp_cc(raw, 1 << spec->resolution);

		r->valid = cc != NTC_INVALID;
		r->cc = r->valid ? cc : 0;
		r->at_ms = now;
	}
}

void power_hw_ntcs(struct ntc_set *out, uint32_t now_ms)
{
	/* no filter caps on the NTC nodes: 10 us acquisition covers the 5 kOhm sources */
	(void)gpio_pin_set_dt(&ntc_pwr, 1);
	ntc_one(&adc_nt1, &out->skin, now_ms);
	ntc_one(&adc_nt3, &out->top, now_ms);
	(void)gpio_pin_set_dt(&ntc_pwr, 0);
}

bool power_hw_chrg_n_low(void)
{
	return gpio_pin_get_dt(&chrg_n) == 1;
}

int power_hw_chg_inhibit(bool inhibit)
{
	return gpio_pin_configure_dt(&chg_inh, inhibit ? GPIO_OUTPUT_ACTIVE : GPIO_DISCONNECTED);
}

int power_hw_ship(void)
{
	LOG_WRN("ship mode");
	LOG_PANIC();
	int ret = gpio_pin_set_dt(&ship, 1);

	if (ret) {
		return ret;
	}
	k_busy_wait(SHIP_HOLD_MS * 1000);
	(void)gpio_pin_set_dt(&ship, 0);
	return -EAGAIN;
}
