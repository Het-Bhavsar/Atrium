/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sensors.h"

#include <math.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "mlc.h"
#include "ppg.h"
#include "pulse.h"
#include "spo2.h"

LOG_MODULE_REGISTER(sensors, CONFIG_ATRIUM_LOG_LEVEL);

static const struct device *const imu = DEVICE_DT_GET(DT_ALIAS(imu0));
static const struct i2c_dt_spec imu_i2c = I2C_DT_SPEC_GET(DT_ALIAS(imu0));

static K_MUTEX_DEFINE(lock);
/* the MLC read switches the IMU to its embedded register page: nothing else may talk to the
 * IMU in between, so the driver's ODR and trigger calls take this too */
static K_MUTEX_DEFINE(imu_bus);
static struct pulse_est pulse;
static struct hrv hrv;
static struct activity act;
static int green_slot = -1;
static int64_t green_sum;
static uint32_t green_n;
static int32_t green_dc;
static int64_t g_sum[3];
static uint32_t g_n;
/* latest accelerometer sample, held for each PPG sample (12.5 Hz against 25 Hz).
 * The hold matches interpolation on the 44 walk and run records (ML_PLAN.md 4). */
static int16_t acc_now[3];
static bool acc_valid;
static sensors_motion_sink_t motion_sink;
static int16_t mbuf[SENSORS_MOTION_BATCH][3];
static uint8_t mbuf_n;
static uint32_t mbuf_t0;
static uint16_t imu_rate_mhz;

/* MLC motion level (mlc.h): read every MLC_WINDOW samples, the IMU's own update period */
static bool mlc_ok;
static uint8_t mlc_count;
static uint8_t mlc_latest;
static bool mlc_new;
static struct mlc_record mlc_rec;
static float norm_ring[MLC_RHYTHM_N];
static uint8_t norm_head;
static uint8_t norm_n;

/* oxygen-trend burst (spo2.h) on top of the applied mode */
static struct mode_cfg applied_cfg;
static bool applied_valid;
static struct spo2 ox;
static bool ox_active, ox_ready;
static int32_t ox_ir;
static bool ox_have_ir;

/* burst slots follow the green slot: IR, then red (ppg order within a sample) */
#define OX_IR_SLOT  1
#define OX_RED_SLOT 2

static void ox_sample(uint8_t slot, int32_t v)
{
	if (slot == OX_IR_SLOT) {
		ox_ir = v;
		ox_have_ir = true;
	} else if (slot == OX_RED_SLOT && ox_have_ir) {
		spo2_push(&ox, v, ox_ir);
		ox_have_ir = false;
		if (spo2_done(&ox)) {
			ox_active = false;
			ox_ready = true;
		}
	}
}

static void ppg_sample(uint8_t slot, uint8_t channel, int32_t v)
{
	if (channel != 0) {
		return;
	}
	k_mutex_lock(&lock, K_FOREVER);
	if ((int)slot == green_slot) {
		if (pulse_push(&pulse, v, acc_valid ? acc_now : NULL) && ox_active) {
			struct pulse_out po;

			/* this second's motion and pulse gate the burst windows */
			if (pulse_get(&pulse, &po)) {
				spo2_gate(&ox, po.motion_mg, po.pulse_present);
			}
		}
		hrv_push(&hrv, v);
		green_sum += v;
		green_n++;
	} else if (ox_active) {
		ox_sample(slot, v);
	}
	k_mutex_unlock(&lock);
}

static void mlc_read(void)
{
	uint8_t lv = 0;
	int ret;

	k_mutex_lock(&imu_bus, K_FOREVER);
	ret = i2c_reg_write_byte_dt(&imu_i2c, MLC_REG_FUNC_CFG_ACCESS, 0x80);
	ret = ret ? ret : i2c_reg_read_byte_dt(&imu_i2c, MLC_REG_MLC0_SRC, &lv);
	/* always back to the main page */
	int ret2 = i2c_reg_write_byte_dt(&imu_i2c, MLC_REG_FUNC_CFG_ACCESS, 0x00);

	k_mutex_unlock(&imu_bus);
	if (ret || ret2 || lv >= MLC_LEVELS) {
		return;
	}
	k_mutex_lock(&lock, K_FOREVER);
	mlc_latest = lv;
	mlc_new = true;
	mlc_record_add(&mlc_rec, lv);
	k_mutex_unlock(&lock);
}

static void imu_ready(const struct device *dev, const struct sensor_trigger *trig)
{
	struct sensor_value a[3];

	ARG_UNUSED(trig);
	if (sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ) ||
	    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, a)) {
		return;
	}
	k_mutex_lock(&lock, K_FOREVER);
	int32_t mg[3];

	for (int i = 0; i < 3; i++) {
		mg[i] = (int32_t)(sensor_value_to_milli(&a[i]) * 1000 / 9807);
		g_sum[i] += mg[i];
		acc_now[i] = (int16_t)CLAMP(mg[i], INT16_MIN, INT16_MAX);
	}
	acc_valid = true;
	g_n++;
	activity_push(&act, mg[0], mg[1], mg[2]);
	norm_ring[norm_head] = sqrtf((float)(mg[0] * mg[0] + mg[1] * mg[1] + mg[2] * mg[2])) / 1000.0f;
	norm_head = (uint8_t)((norm_head + 1) % MLC_RHYTHM_N);
	norm_n = (uint8_t)MIN(norm_n + 1, MLC_RHYTHM_N);
	bool mlc_due = mlc_ok && ++mlc_count >= MLC_WINDOW;

	if (mlc_due) {
		mlc_count = 0;
	}

	sensors_motion_sink_t sink = motion_sink;
	bool flush = false;

	if (sink) {
		if (mbuf_n == 0) {
			mbuf_t0 = k_uptime_get_32();
		}
		memcpy(mbuf[mbuf_n++], acc_now, sizeof(acc_now));
		flush = mbuf_n == SENSORS_MOTION_BATCH;
	}
	k_mutex_unlock(&lock);
	if (flush) {
		sink((const int16_t (*)[3])mbuf, mbuf_n, mbuf_t0, imu_rate_mhz);
		mbuf_n = 0;
	}
	if (mlc_due) {
		mlc_read();
	}
}

static int find_led(const struct mode_cfg *m, uint8_t led)
{
	for (uint8_t i = 0; i < m->n_slots; i++) {
		if (m->slot[i].led == led) {
			return i;
		}
	}
	return -1;
}

static int ppg_mode(const struct mode_cfg *m)
{
	/* pulse source: green, else IR, else red (when headroom drops green) */
	int slot = find_led(m, LED_GREEN);

	if (slot < 0) {
		slot = find_led(m, LED_IR);
	}
	if (slot < 0) {
		slot = find_led(m, LED_RED);
	}
	uint8_t green_hz = slot >= 0 ? m->hz : 0;

	k_mutex_lock(&lock, K_FOREVER);
	green_slot = slot;
	green_sum = 0;
	green_n = 0;
	pulse_init(&pulse, green_hz);
	hrv_init(&hrv, green_hz);
	k_mutex_unlock(&lock);
	return ppg_apply(m);
}

static int imu_apply(const struct mode_cfg *m)
{
	struct sensor_value odr = {.val1 = m->imu_mhz / 1000, .val2 = (m->imu_mhz % 1000) * 1000};
	struct sensor_trigger trig = {.type = SENSOR_TRIG_DATA_READY,
				      .chan = SENSOR_CHAN_ACCEL_XYZ};

	if (!device_is_ready(imu)) {
		return -ENODEV;
	}
	k_mutex_lock(&imu_bus, K_FOREVER);
	if (m->imu_mhz == 0) {
		(void)sensor_trigger_set(imu, &trig, NULL);
	}
	int ret = sensor_attr_set(imu, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);

	if (ret == 0 && m->imu_mhz) {
		ret = sensor_trigger_set(imu, &trig, imu_ready);
	}
	k_mutex_unlock(&imu_bus);
	return ret;
}

int sensors_init(void)
{
	activity_init(&act);
	pulse_init(&pulse, 50);
	hrv_init(&hrv, 50);
	ppg_set_callbacks(ppg_sample, NULL);
	if (!ppg_ready()) {
		LOG_ERR("PPG front end not ready");
	}
	if (!device_is_ready(imu)) {
		LOG_ERR("LSM6DSOX not ready");
		return 0;
	}
	/* the MLC program, after the driver's init. It leaves the accelerometer at 12.5 Hz,
	 * +-2 g; the modes set only the ODR (12.5 Hz worn, off docked) */
	int ret = 0;

	for (size_t i = 0; i < mlc_program_len && !ret; i++) {
		ret = i2c_reg_write_byte_dt(&imu_i2c, mlc_program[i].reg, mlc_program[i].val);
	}
	mlc_ok = ret == 0;
	if (ret) {
		(void)i2c_reg_write_byte_dt(&imu_i2c, MLC_REG_FUNC_CFG_ACCESS, 0x00);
		LOG_ERR("MLC program %d", ret);
	}
	mlc_record_reset(&mlc_rec);
	return 0;
}

int sensors_set_mode(enum pod_mode mode, bool green_ok, bool optics_on)
{
	const struct mode_cfg *m = mode_get(mode);
	struct mode_cfg low;

	if (!m) {
		return -EINVAL;
	}
	if (!green_ok) {
		(void)mode_green_to_ir(m, &low);
		m = &low;
	}
	imu_rate_mhz = m->imu_mhz;
	if (m->imu_mhz == 0) {
		acc_valid = false;
	}
	if (!optics_on) {
		low = *m;
		low.n_slots = 0; /* duty-cycled mode, off phase: IMU keeps running */
		m = &low;
	}
	k_mutex_lock(&lock, K_FOREVER);
	ox_active = false; /* a mode change ends any burst */
	ox_ready = false;
	applied_cfg = *m;
	applied_valid = true;
	if (m->imu_mhz == 0) {
		norm_n = 0;
		mlc_count = 0;
	}
	k_mutex_unlock(&lock);
	int a = ppg_mode(m);
	int b = imu_apply(m);

	LOG_INF("mode %s (afe %d, imu %d)", m->name, a, b);
	return a ? a : b;
}

bool sensors_imu_ready(void)
{
	return device_is_ready(imu);
}

void sensors_set_motion_sink(sensors_motion_sink_t sink)
{
	k_mutex_lock(&lock, K_FOREVER);
	motion_sink = sink;
	mbuf_n = 0;
	k_mutex_unlock(&lock);
}

void sensors_set_ppg_sink(sensors_ppg_sink_t sink)
{
	ppg_set_callbacks(ppg_sample, sink);
}

bool sensors_green_dc(int32_t *dc)
{
	bool ok;

	k_mutex_lock(&lock, K_FOREVER);
	ok = green_n > 0;
	if (ok) {
		green_dc = (int32_t)(green_sum / green_n);
	}
	*dc = green_dc;
	green_sum = 0;
	green_n = 0;
	k_mutex_unlock(&lock);
	return ok;
}

bool sensors_pulse(struct pulse_out *out)
{
	k_mutex_lock(&lock, K_FOREVER);
	bool ok = pulse_get(&pulse, out);

	k_mutex_unlock(&lock);
	return ok;
}

void sensors_posture_close(int16_t *pitch_deg, int16_t *roll_deg)
{
	k_mutex_lock(&lock, K_FOREVER);
	posture_angles(g_sum[0], g_sum[1], g_sum[2], g_n, pitch_deg, roll_deg);
	g_sum[0] = g_sum[1] = g_sum[2] = 0;
	g_n = 0;
	k_mutex_unlock(&lock);
}

struct hrv_window sensors_hrv_close(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	/* beats up to half a second ago are final */
	struct hrv_window w = hrv_close(&hrv, hrv_time_s(&hrv) - 0.5);

	k_mutex_unlock(&lock);
	return w;
}

struct activity_minute sensors_activity_minute(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	struct activity_minute m = activity_close(&act);

	k_mutex_unlock(&lock);
	return m;
}

bool sensors_mlc_poll(uint8_t *level)
{
	k_mutex_lock(&lock, K_FOREVER);
	bool n = mlc_new;

	*level = mlc_latest;
	mlc_new = false;
	k_mutex_unlock(&lock);
	return n;
}

struct sensors_motion sensors_motion_close(void)
{
	float norm[MLC_RHYTHM_N];
	struct mlc_record r;
	struct sensors_motion out = {0};
	uint8_t n;

	k_mutex_lock(&lock, K_FOREVER);
	r = mlc_rec;
	mlc_record_reset(&mlc_rec);
	n = norm_n;
	for (uint8_t i = 0; i < n; i++) {
		norm[i] = norm_ring[(norm_head + MLC_RHYTHM_N - n + i) % MLC_RHYTHM_N];
	}
	k_mutex_unlock(&lock);

	float spm = 0.0f, share = 0.0f;
	bool ok = n == MLC_RHYTHM_N &&
		  mlc_step_rhythm(norm, n, (float)MLC_FS_MHZ / 1000.0f, &spm, &share);

	out.max = mlc_record_max(&r);
	out.median = mlc_record_median(&r);
	out.cls = (uint8_t)mlc_activity_class(&r, ok, spm, share);
	out.steps_pm = ok && out.max > 0 ? (uint8_t)CLAMP(lroundf(spm), 0, 255) : 0;
	return out;
}

int sensors_spo2_start(void)
{
	struct spo2_cfg cfg = SPO2_CFG_DEFAULT;
	struct mode_cfg b;
	int ret;

	k_mutex_lock(&lock, K_FOREVER);
	b = applied_cfg;
	/* one green slot at 25 Hz in front; IR and red join it, green stays slot 0 so the
	 * pulse and HRV estimates run on undisturbed */
	if (!applied_valid || ox_active || b.n_slots != 1 || b.slot[0].led != LED_GREEN ||
	    b.hz != SPO2_FS || green_slot != 0) {
		k_mutex_unlock(&lock);
		return -ENOTSUP;
	}
	b.slot[OX_IR_SLOT] = (struct led_slot){.led = LED_IR, .ma = SPO2_IR_MA};
	b.slot[OX_RED_SLOT] = (struct led_slot){.led = LED_RED, .ma = SPO2_RED_MA};
	b.n_slots = 3;
	spo2_init(&ox, &cfg);
	ox_have_ir = false;
	ox_ready = false;
	ox_active = true;
	k_mutex_unlock(&lock);
	ret = ppg_apply(&b);
	if (ret) {
		k_mutex_lock(&lock, K_FOREVER);
		ox_active = false;
		k_mutex_unlock(&lock);
	}
	return ret;
}

bool sensors_spo2_poll(struct spo2_out *out)
{
	struct mode_cfg m;

	k_mutex_lock(&lock, K_FOREVER);
	bool done = ox_ready;

	if (done) {
		spo2_result(&ox, out);
		ox_ready = false;
	}
	m = applied_cfg;
	k_mutex_unlock(&lock);
	if (done) {
		/* back to the mode's own slots; green stays slot 0, so no estimate restarts */
		(void)ppg_apply(&m);
	}
	return done;
}

bool sensors_spo2_active(void)
{
	return ox_active;
}
