/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT maxim_maxm86161

#include "maxm86161.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(maxm86161, CONFIG_MAXM86161_LOG_LEVEL);

#define CHUNK_ITEMS 32
/* Time from LDO_EN high to the first I2C access. The data sheet gives no figure;
 * 2 ms covers the internal LDO with its 1 uF on VLDO [ASSUMED, VERIFY on the bench]. */
#define LDO_ON_MS 2

struct maxm86161_cfg {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec irq;
	struct gpio_dt_spec en;
};

struct maxm86161_data {
	const struct device *dev;
	struct k_mutex lock;
	struct k_work work;
	struct gpio_callback gpio_cb;
	maxm86161_fifo_cb_t cb;
	void *user_data;
	bool on;
	uint8_t buf[CHUNK_ITEMS * 3];
};

static int wr(const struct device *dev, uint8_t reg, uint8_t val)
{
	const struct maxm86161_cfg *cfg = dev->config;

	return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

static int rd(const struct device *dev, uint8_t reg, uint8_t *val)
{
	const struct maxm86161_cfg *cfg = dev->config;

	return i2c_reg_read_byte_dt(&cfg->i2c, reg, val);
}

static void drain_fifo(const struct device *dev)
{
	const struct maxm86161_cfg *cfg = dev->config;
	struct maxm86161_data *data = dev->data;
	uint8_t ovf, count;

	/* the data sheet reads OVF_COUNTER just before the FIFO */
	if (rd(dev, MAXM86161_REG_OVF_COUNTER, &ovf) || rd(dev, MAXM86161_REG_FIFO_COUNT, &count)) {
		return;
	}
	if (ovf) {
		LOG_WRN("FIFO overflow, %u items lost", ovf);
	}
	while (count > 0) {
		uint8_t n = MIN(count, CHUNK_ITEMS);

		/* burst read: each 3 bytes advance FIFO_RD_PTR [VERIFY multi-item bursts] */
		if (i2c_burst_read_dt(&cfg->i2c, MAXM86161_REG_FIFO_DATA, data->buf, n * 3)) {
			return;
		}
		if (data->cb) {
			data->cb(dev, data->buf, n, data->user_data);
		}
		count -= n;
	}
}

static void work_handler(struct k_work *work)
{
	struct maxm86161_data *data = CONTAINER_OF(work, struct maxm86161_data, work);
	uint8_t st;

	k_mutex_lock(&data->lock, K_FOREVER);
	if (data->on && rd(data->dev, MAXM86161_REG_INT_STATUS1, &st) == 0 &&
	    (st & (MAXM86161_INT_A_FULL | MAXM86161_INT_ALC_OVF))) {
		drain_fifo(data->dev);
	}
	k_mutex_unlock(&data->lock);
}

static void irq_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	struct maxm86161_data *data = CONTAINER_OF(cb, struct maxm86161_data, gpio_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);
	k_work_submit(&data->work);
}

int maxm86161_off(const struct device *dev)
{
	const struct maxm86161_cfg *cfg = dev->config;
	struct maxm86161_data *data = dev->data;

	k_mutex_lock(&data->lock, K_FOREVER);
	if (data->on) {
		(void)wr(dev, MAXM86161_REG_SYS_CTRL, MAXM86161_SHDN | MAXM86161_SINGLE_PPG);
	}
	data->on = false;
	data->cb = NULL;
	int ret = gpio_pin_set_dt(&cfg->en, 0);

	k_mutex_unlock(&data->lock);
	return ret;
}

int maxm86161_start(const struct device *dev, const struct maxm86161_slot *slots, size_t n,
		    uint16_t hz, maxm86161_fifo_cb_t cb, void *user_data)
{
	const struct maxm86161_cfg *cfg = dev->config;
	struct maxm86161_data *data = dev->data;
	struct maxm86161_cfg_regs r;
	uint8_t id, st;
	int ret;

	if (maxm86161_encode(slots, n, hz, &r)) {
		return -EINVAL;
	}
	k_mutex_lock(&data->lock, K_FOREVER);
	ret = gpio_pin_set_dt(&cfg->en, 1);
	k_sleep(K_MSEC(LDO_ON_MS));
	ret = ret ? ret : rd(dev, MAXM86161_REG_PART_ID, &id);
	if (ret == 0 && id != MAXM86161_PART_ID) {
		LOG_ERR("part id 0x%02x, expected 0x%02x", id, MAXM86161_PART_ID);
		ret = -ENODEV;
	}
	/* the data sheet's start-up sequence: reset, 1 ms, shutdown, clear status, configure */
	ret = ret ? ret : wr(dev, MAXM86161_REG_SYS_CTRL, MAXM86161_RESET);
	k_sleep(K_MSEC(1));
	ret = ret ? ret : wr(dev, MAXM86161_REG_SYS_CTRL, MAXM86161_SHDN | MAXM86161_SINGLE_PPG);
	ret = ret ? ret : rd(dev, MAXM86161_REG_INT_STATUS1, &st);
	ret = ret ? ret : rd(dev, MAXM86161_REG_INT_STATUS2, &st);
	/* tINT 117.3 us, ADC 16 uA full scale (data sheet example) */
	ret = ret ? ret : wr(dev, MAXM86161_REG_PPG_CFG1, (0x2 << 2) | 0x3);
	ret = ret ? ret : wr(dev, MAXM86161_REG_PPG_CFG2, r.ppg_cfg2);
	ret = ret ? ret : wr(dev, MAXM86161_REG_PPG_CFG3, 0x3 << 6); /* LED settling 12 us, CDM */
	ret = ret ? ret : wr(dev, MAXM86161_REG_PD_BIAS, 0x1);       /* PD 0 to 65 pF */
	ret = ret ? ret : wr(dev, MAXM86161_REG_LED_RANGE1, r.range1);
	for (int i = 0; i < 3 && !ret; i++) {
		ret = wr(dev, MAXM86161_REG_LED1_PA + i, r.pa[i]);
		ret = ret ? ret : wr(dev, MAXM86161_REG_LED_SEQ1 + i, r.seq[i]);
	}
	/* interrupt about once a second, at most half the FIFO */
	uint32_t items = CLAMP((uint32_t)n * hz, 8U, MAXM86161_FIFO_DEPTH / 2U);

	ret = ret ? ret : wr(dev, MAXM86161_REG_FIFO_CFG1, (uint8_t)(MAXM86161_FIFO_DEPTH - items));
	ret = ret ? ret : wr(dev, MAXM86161_REG_FIFO_CFG2, MAXM86161_FIFO_RO |
			     MAXM86161_FIFO_STAT_CLR | MAXM86161_A_FULL_TYPE | MAXM86161_FLUSH_FIFO);
	ret = ret ? ret : wr(dev, MAXM86161_REG_INT_EN1, MAXM86161_INT_A_FULL);
	data->cb = cb;
	data->user_data = user_data;
	/* low-power mode powers down between samples (256 sps and below) */
	ret = ret ? ret : wr(dev, MAXM86161_REG_SYS_CTRL, MAXM86161_LP_MODE | MAXM86161_SINGLE_PPG);
	data->on = ret == 0;
	k_mutex_unlock(&data->lock);
	if (ret) {
		(void)maxm86161_off(dev);
	}
	return ret;
}

static int maxm86161_init(const struct device *dev)
{
	const struct maxm86161_cfg *cfg = dev->config;
	struct maxm86161_data *data = dev->data;
	int ret;

	data->dev = dev;
	k_mutex_init(&data->lock);
	k_work_init(&data->work, work_handler);
	if (!i2c_is_ready_dt(&cfg->i2c) || !gpio_is_ready_dt(&cfg->irq) ||
	    !gpio_is_ready_dt(&cfg->en)) {
		return -ENODEV;
	}
	/* off until a mode needs it */
	ret = gpio_pin_configure_dt(&cfg->en, GPIO_OUTPUT_INACTIVE);
	ret = ret ? ret : gpio_pin_configure_dt(&cfg->irq, GPIO_INPUT);
	ret = ret ? ret : gpio_pin_interrupt_configure_dt(&cfg->irq, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret) {
		return ret;
	}
	gpio_init_callback(&data->gpio_cb, irq_handler, BIT(cfg->irq.pin));
	return gpio_add_callback(cfg->irq.port, &data->gpio_cb);
}

#define MAXM86161_DEFINE(inst)                                                    \
	static struct maxm86161_data maxm86161_data_##inst;                         \
	static const struct maxm86161_cfg maxm86161_cfg_##inst = {                  \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                  \
		.irq = GPIO_DT_SPEC_INST_GET(inst, int_gpios),                      \
		.en = GPIO_DT_SPEC_INST_GET(inst, en_gpios),                        \
	};                                                                          \
	DEVICE_DT_INST_DEFINE(inst, maxm86161_init, NULL, &maxm86161_data_##inst,   \
			      &maxm86161_cfg_##inst, POST_KERNEL,                   \
			      CONFIG_MAXM86161_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MAXM86161_DEFINE)
