/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "charger.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/reboot.h>

#include "bq21061_regs.h"
#include "vinpat.h"

LOG_MODULE_REGISTER(charger, CONFIG_ATRIUM_LOG_LEVEL);

#define CHG DT_ALIAS(charger0)

static const struct i2c_dt_spec bus = I2C_DT_SPEC_GET(CHG);
static const struct gpio_dt_spec pg = GPIO_DT_SPEC_GET(CHG, pg_gpios);
static bool id_ok;
static struct gpio_callback pg_cb;
static struct vinpat pat;
static atomic_t vin_good;

/* Every /PG edge, in interrupt context, so the case's soft reset works with the threads
 * hung. The reboot is a plain SYSRESETREQ: the log is written last word first
 * (logpack.h), so a reset mid-append leaves no torn record. */
static void pg_edge(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	bool vin = gpio_pin_get_dt(&pg) == 1;

	if (vin) {
		atomic_set(&vin_good, 1);
	}
	if (vinpat_edge(&pat, vin, k_uptime_get_32()) == VINPAT_RESET) {
		sys_reboot(SYS_REBOOT_COLD);
	}
}

int charger_init(void)
{
	static const struct vinpat_cfg cfg = VINPAT_CFG_DEFAULT;
	int ret;

	if (!gpio_is_ready_dt(&pg) || !i2c_is_ready_dt(&bus)) {
		return -ENODEV;
	}
	/* R3 100 kOhm pull-up on the board */
	ret = gpio_pin_configure_dt(&pg, GPIO_INPUT);
	if (ret) {
		return ret;
	}
	vinpat_init(&pat, &cfg, gpio_pin_get_dt(&pg) == 1);
	atomic_set(&vin_good, gpio_pin_get_dt(&pg) == 1);
	gpio_init_callback(&pg_cb, pg_edge, BIT(pg.pin));
	ret = gpio_add_callback_dt(&pg, &pg_cb);
	/* both edges by SENSE (sense-edge-mask in the board DTS), not a GPIOTE channel */
	return ret ? ret : gpio_pin_interrupt_configure_dt(&pg, GPIO_INT_EDGE_BOTH);
}

bool charger_vin_good_edge(void)
{
	return atomic_cas(&vin_good, 1, 0);
}

bool charger_on_case(void)
{
	unsigned int key = irq_lock();
	bool on = vinpat_on_case(&pat);

	irq_unlock(key);
	return on;
}

bool charger_pg_low(void)
{
	return gpio_pin_get_dt(&pg) == 1;
}

static int check_id(void)
{
	uint8_t id;
	int ret = i2c_reg_read_byte_dt(&bus, BQ21061_DEVICE_ID, &id);

	if (ret == 0 && id != BQ21061_DEVICE_ID_VAL) {
		LOG_ERR("charger id 0x%02x", id);
		return -ENODEV;
	}
	return ret;
}

int charger_service(bool cold, bool disable, struct chg_report *out, uint8_t *stat0,
		    uint8_t *stat1)
{
	struct bq21061_write w[4];
	struct bq21061_status st;
	uint8_t s[2] = {0, 0};
	uint8_t ic2;
	uint32_t ua = cold ? DT_PROP(CHG, cold_charge_current_microamp)
			   : DT_PROP(CHG, constant_charge_current_max_microamp);
	size_t n = bq21061_config(ua, w, ARRAY_SIZE(w));
	int ret = id_ok ? 0 : check_id();

	id_ok = ret == 0;
	for (size_t i = 0; i < n && !ret; i++) {
		ret = i2c_reg_write_byte_dt(&bus, w[i].reg, w[i].val);
	}
	ret = ret ? ret : i2c_reg_read_byte_dt(&bus, BQ21061_ICCTRL2, &ic2);
	ret = ret ? ret : i2c_reg_write_byte_dt(&bus, BQ21061_ICCTRL2, bq21061_icctrl2(ic2, disable));
	ret = ret ? ret : i2c_burst_read_dt(&bus, BQ21061_STAT0, s, sizeof(s));

	*out = (struct chg_report){0};
	*stat0 = ret ? 0 : s[0];
	*stat1 = ret ? 0 : s[1];
	if (ret) {
		id_ok = false; /* check again once it answers */
		return ret;
	}
	bq21061_decode(s[0], s[1], &st);
	out->valid = true;
	out->done = st.done;
	out->ts_stop = st.ts_cold || st.ts_hot;
	out->fault = st.fault;
	return 0;
}

int charger_ship_on_lift(void)
{
	uint8_t v;
	int ret = i2c_reg_read_byte_dt(&bus, BQ21061_ICCTRL0, &v);

	return ret ? ret : i2c_reg_write_byte_dt(&bus, BQ21061_ICCTRL0, v | BQ21061_ICCTRL0_EN_SHIP_MODE);
}
