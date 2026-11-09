/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "power_hw.h"

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/poweroff.h>

#include "ntc.h"

LOG_MODULE_REGISTER(power_hw, CONFIG_ATRIUM_LOG_LEVEL);

#define USER DT_PATH(zephyr_user)

static const struct adc_dt_spec adc_th1 = ADC_DT_SPEC_GET_BY_NAME(USER, th1_skin);
static const struct adc_dt_spec adc_th4 = ADC_DT_SPEC_GET_BY_NAME(USER, th4_top);
static const struct adc_dt_spec adc_vsys = ADC_DT_SPEC_GET_BY_NAME(USER, vsys);
static const struct gpio_dt_spec ntc_en = GPIO_DT_SPEC_GET(USER, ntc_en_gpios);
static const struct gpio_dt_spec pg = GPIO_DT_SPEC_GET(DT_ALIAS(charger0), pg_gpios);

#define VSYS_TOP_OHM DT_PROP(USER, vsys_sense_top_ohm)
#define VSYS_BOT_OHM DT_PROP(USER, vsys_sense_bottom_ohm)

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

int power_hw_init(void)
{
	const struct adc_dt_spec *ch[] = {&adc_th1, &adc_th4, &adc_vsys};
	int ret = 0;

	if (!adc_is_ready_dt(&adc_vsys) || !gpio_is_ready_dt(&ntc_en)) {
		return -ENODEV;
	}
	for (size_t i = 0; i < ARRAY_SIZE(ch) && !ret; i++) {
		ret = adc_channel_setup_dt(ch[i]);
	}
	/* NTC dividers unpowered between readings */
	return ret ? ret : gpio_pin_configure_dt(&ntc_en, GPIO_OUTPUT_INACTIVE);
}

int power_hw_vsys_mv(uint16_t *mv)
{
	int32_t v;
	int ret = read_adc(&adc_vsys, &v);

	ret = ret ? ret : adc_raw_to_millivolts_dt(&adc_vsys, &v);
	if (ret == 0) {
		int64_t vsys = (int64_t)(v < 0 ? 0 : v) * (VSYS_TOP_OHM + VSYS_BOT_OHM) / VSYS_BOT_OHM;

		*mv = vsys > UINT16_MAX ? UINT16_MAX : (uint16_t)vsys;
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
	/* C10 / C11 (10 nF, fitted only if EVT noise needs them) behind about 5 kOhm:
	 * 500 us is 10 time constants. Without them the 10 us acquisition is enough. */
	(void)gpio_pin_set_dt(&ntc_en, 1);
	k_busy_wait(500);
	ntc_one(&adc_th1, &out->skin, now_ms);
	ntc_one(&adc_th4, &out->top, now_ms);
	(void)gpio_pin_set_dt(&ntc_en, 0);
}

void power_hw_poweroff(void)
{
	LOG_WRN("power off until the Plinth");
	LOG_PANIC();
	(void)gpio_pin_set_dt(&ntc_en, 0);
	/* SENSE on /PG: the GPIO DETECT signal wakes the nRF52 from System OFF */
	(void)gpio_pin_configure_dt(&pg, GPIO_INPUT);
	(void)gpio_pin_interrupt_configure_dt(&pg, GPIO_INT_LEVEL_ACTIVE);
	sys_poweroff();
}
