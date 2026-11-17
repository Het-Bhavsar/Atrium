/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * LSM6DSV16BX accelerometer (pod U11) as a Zephyr sensor: the output rate, data-ready on INT1,
 * and x, y, z in m/s^2 at +-2 g. Only what the pod uses. The gyroscope, TDM, Qvar and the
 * embedded functions stay off. The vibration capture (src/vibcap.h) writes the FIFO registers
 * itself between modes; this driver rewrites CTRL1 whole at each rate change, so it starts
 * clean after one.
 */
#define DT_DRV_COMPAT st_lsm6dsv16bx

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "lsm6dsv16bx_regs.h"

LOG_MODULE_REGISTER(lsm6dsv16bx, CONFIG_SENSOR_LOG_LEVEL);

/* Power-up to the first I2C access. [VERIFY DS13771 turn-on time]; the IMU rail (U13) is up
 * long before the application starts, so this only matters on a cold boot. */
#define BOOT_MS 10
#define RESET_POLL_US 100
#define RESET_TRIES 100

struct lsm6dsv16bx_cfg {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec int1;
	uint32_t lp_max_mhz;
};

struct lsm6dsv16bx_data {
	const struct device *dev;
	struct gpio_callback cb;
	struct k_work work;
	sensor_trigger_handler_t handler;
	const struct sensor_trigger *trig;
	int16_t xyz[3];
	uint32_t odr_mhz;
};

static int wr(const struct device *dev, uint8_t reg, uint8_t val)
{
	const struct lsm6dsv16bx_cfg *cfg = dev->config;

	return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

static int lsm6dsv16bx_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct lsm6dsv16bx_cfg *cfg = dev->config;
	struct lsm6dsv16bx_data *data = dev->data;
	uint8_t raw[6];

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_ACCEL_XYZ) {
		return -ENOTSUP;
	}
	int ret = i2c_burst_read_dt(&cfg->i2c, LSM6DSV16BX_REG_OUTZ_L_A, raw, sizeof(raw));

	if (ret == 0) {
		lsm6dsv16bx_xyz(raw, data->xyz);
	}
	return ret;
}

static void to_value(int16_t lsb, struct sensor_value *v)
{
	int32_t u = lsm6dsv16bx_lsb_to_ums2(lsb);

	v->val1 = u / 1000000;
	v->val2 = u % 1000000;
}

static int lsm6dsv16bx_channel_get(const struct device *dev, enum sensor_channel chan,
				   struct sensor_value *val)
{
	struct lsm6dsv16bx_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_ACCEL_XYZ:
		for (int i = 0; i < 3; i++) {
			to_value(data->xyz[i], &val[i]);
		}
		return 0;
	case SENSOR_CHAN_ACCEL_X:
	case SENSOR_CHAN_ACCEL_Y:
	case SENSOR_CHAN_ACCEL_Z:
		to_value(data->xyz[chan - SENSOR_CHAN_ACCEL_X], val);
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int lsm6dsv16bx_attr_set(const struct device *dev, enum sensor_channel chan,
				enum sensor_attribute attr, const struct sensor_value *val)
{
	const struct lsm6dsv16bx_cfg *cfg = dev->config;
	struct lsm6dsv16bx_data *data = dev->data;

	if (chan != SENSOR_CHAN_ACCEL_XYZ || attr != SENSOR_ATTR_SAMPLING_FREQUENCY ||
	    val->val1 < 0 || val->val2 < 0) {
		return -ENOTSUP;
	}
	uint32_t mhz = (uint32_t)val->val1 * 1000U + (uint32_t)val->val2 / 1000U;
	int c1 = lsm6dsv16bx_ctrl1(mhz, cfg->lp_max_mhz);

	if (c1 < 0) {
		LOG_ERR("no %u mHz rate", mhz);
		return -EINVAL;
	}
	int ret = wr(dev, LSM6DSV16BX_REG_CTRL1, (uint8_t)c1);

	if (ret == 0) {
		data->odr_mhz = mhz;
	}
	return ret;
}

static void lsm6dsv16bx_work(struct k_work *work)
{
	struct lsm6dsv16bx_data *data = CONTAINER_OF(work, struct lsm6dsv16bx_data, work);
	sensor_trigger_handler_t h = data->handler;

	if (h) {
		h(data->dev, data->trig);
	}
}

static void lsm6dsv16bx_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	struct lsm6dsv16bx_data *data = CONTAINER_OF(cb, struct lsm6dsv16bx_data, cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);
	k_work_submit(&data->work);
}

static int lsm6dsv16bx_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
				   sensor_trigger_handler_t handler)
{
	const struct lsm6dsv16bx_cfg *cfg = dev->config;
	struct lsm6dsv16bx_data *data = dev->data;

	if (trig->type != SENSOR_TRIG_DATA_READY ||
	    (trig->chan != SENSOR_CHAN_ACCEL_XYZ && trig->chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}
	(void)gpio_pin_interrupt_configure_dt(&cfg->int1, GPIO_INT_DISABLE);
	data->handler = handler;
	data->trig = trig;
	int ret = wr(dev, LSM6DSV16BX_REG_INT1_CTRL, handler ? LSM6DSV16BX_INT1_DRDY_XL : 0);

	if (ret == 0 && handler) {
		ret = gpio_pin_interrupt_configure_dt(&cfg->int1, GPIO_INT_EDGE_TO_ACTIVE);
	}
	return ret;
}

static DEVICE_API(sensor, lsm6dsv16bx_api) = {
	.sample_fetch = lsm6dsv16bx_sample_fetch,
	.channel_get = lsm6dsv16bx_channel_get,
	.attr_set = lsm6dsv16bx_attr_set,
	.trigger_set = lsm6dsv16bx_trigger_set,
};

static int lsm6dsv16bx_init(const struct device *dev)
{
	const struct lsm6dsv16bx_cfg *cfg = dev->config;
	struct lsm6dsv16bx_data *data = dev->data;
	uint8_t id = 0, c3 = LSM6DSV16BX_CTRL3_SW_RESET;
	int ret;

	data->dev = dev;
	k_work_init(&data->work, lsm6dsv16bx_work);
	if (!i2c_is_ready_dt(&cfg->i2c) || !gpio_is_ready_dt(&cfg->int1)) {
		return -ENODEV;
	}
	k_msleep(BOOT_MS);
	ret = i2c_reg_read_byte_dt(&cfg->i2c, LSM6DSV16BX_REG_WHO_AM_I, &id);
	if (ret || id != LSM6DSV16BX_WHO_AM_I_VAL) {
		LOG_ERR("WHO_AM_I 0x%02x, want 0x%02x (%d)", id, LSM6DSV16BX_WHO_AM_I_VAL, ret);
		return -ENODEV;
	}
	/* control registers to their reset values, then wait for SW_RESET to clear */
	ret = wr(dev, LSM6DSV16BX_REG_CTRL3, LSM6DSV16BX_CTRL3_SW_RESET);
	for (int i = 0; ret == 0 && (c3 & LSM6DSV16BX_CTRL3_SW_RESET) && i < RESET_TRIES; i++) {
		k_busy_wait(RESET_POLL_US);
		ret = i2c_reg_read_byte_dt(&cfg->i2c, LSM6DSV16BX_REG_CTRL3, &c3);
	}
	if (ret || (c3 & LSM6DSV16BX_CTRL3_SW_RESET)) {
		return ret ? ret : -ETIMEDOUT;
	}
	/* block data update, address auto-increment; pulsed data-ready, so a missed read never
	 * leaves INT1 stuck high; +-2 g; everything off. IF_CFG keeps its reset value: INT1
	 * push-pull, active high, TDMout pull-up off (TDMout is tied to GND). */
	const uint8_t init[][2] = {
		{LSM6DSV16BX_REG_CTRL3, LSM6DSV16BX_CTRL3_BDU | LSM6DSV16BX_CTRL3_IF_INC},
		{LSM6DSV16BX_REG_CTRL4, LSM6DSV16BX_CTRL4_DRDY_PULSED},
		{LSM6DSV16BX_REG_CTRL8, 0x00},
		{LSM6DSV16BX_REG_CTRL7, 0x00},
		{LSM6DSV16BX_REG_CTRL2, 0x00},
		{LSM6DSV16BX_REG_CTRL1, 0x00},
		{LSM6DSV16BX_REG_INT1_CTRL, 0x00},
	};

	for (size_t i = 0; i < ARRAY_SIZE(init) && ret == 0; i++) {
		ret = wr(dev, init[i][0], init[i][1]);
	}
	if (ret == 0) {
		ret = gpio_pin_configure_dt(&cfg->int1, GPIO_INPUT);
	}
	if (ret == 0) {
		gpio_init_callback(&data->cb, lsm6dsv16bx_isr, BIT(cfg->int1.pin));
		ret = gpio_add_callback(cfg->int1.port, &data->cb);
	}
	return ret;
}

#define LSM6DSV16BX_DEFINE(inst)                                                                  \
	static struct lsm6dsv16bx_data lsm6dsv16bx_data_##inst;                                   \
	static const struct lsm6dsv16bx_cfg lsm6dsv16bx_cfg_##inst = {                            \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                \
		.int1 = GPIO_DT_SPEC_INST_GET(inst, int1_gpios),                                  \
		.lp_max_mhz = DT_INST_PROP(inst, low_power_max_mhz),                              \
	};                                                                                        \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, lsm6dsv16bx_init, NULL, &lsm6dsv16bx_data_##inst,      \
				     &lsm6dsv16bx_cfg_##inst, POST_KERNEL,                        \
				     CONFIG_SENSOR_INIT_PRIORITY, &lsm6dsv16bx_api);

DT_INST_FOREACH_STATUS_OKAY(LSM6DSV16BX_DEFINE)
