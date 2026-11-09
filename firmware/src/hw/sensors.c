/*
 * Copyright (c) 2026 Het Bhavsar
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
#include "vibcap.h"

LOG_MODULE_REGISTER(sensors, CONFIG_ATRIUM_LOG_LEVEL);

static const struct device *const imu = DEVICE_DT_GET(DT_ALIAS(imu0));

/* chip-to-pod axes from the board (atrium_pod.dts imu-to-pod); cells are u32, so -1 arrives
 * as 0xFFFFFFFF and the cast brings it back */
#define ROT(i, _) (int8_t)(int32_t)DT_PROP_BY_IDX(DT_PATH(zephyr_user), imu_to_pod, i)
static const int8_t imu_rot[9] = {LISTIFY(9, ROT, (,))};
BUILD_ASSERT(DT_PROP_LEN(DT_PATH(zephyr_user), imu_to_pod) == 9, "imu-to-pod: 9 entries");
static const struct i2c_dt_spec imu_i2c = I2C_DT_SPEC_GET(DT_ALIAS(imu0));

static K_MUTEX_DEFINE(lock);
/* the MLC read switches the IMU to its embedded register page: nothing else may talk to the
 * IMU in between, so the driver's ODR and trigger calls take this too */
static K_MUTEX_DEFINE(imu_bus);
static struct pulse_est pulse;
static struct sigq sq; /* fit-guide signal quality (sigq.h) */
static struct hrv hrv;
static struct activity act;
static struct activity act_epoch; /* the same index over one log epoch */
static int green_slot = -1;       /* the pulse-source slot: green, else IR, else red */
static uint8_t pulse_led;
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
static uint32_t imu_rate_mhz;
/* research modes: PPG samples averaged per 25 Hz filter sample, accelerometer samples per
 * step-rhythm sample (modes.h); 1 in every other mode */
static uint8_t algo_k = 1;
static uint8_t algo_i;
static int64_t algo_acc;
static uint8_t imu_k = 1;
static uint8_t imu_i;

/* MLC motion level (mlc.h): read every MLC_WINDOW samples, the IMU's own update period */
static bool mlc_ok;
static uint8_t mlc_count;
static uint8_t mlc_latest;
static bool mlc_new;
static struct mlc_record mlc_rec;
static float norm_ring[MLC_RHYTHM_N];
static uint8_t norm_head;
static uint8_t norm_n;

/* vibration capture (vibcap.h), research modes 10 and 11: the drain thread reads the FIFO
 * under imu_bus; sensors_set_mode stops it (vib_on 0) and then takes imu_bus, so a burst is
 * never cut */
static enum pod_mode imu_mode = MODE_DOCKED_IDLE; /* the mode the IMU last took */
static struct vib_cap vib;
static atomic_t vib_on;
static atomic_t vib_gen;
static K_SEM_DEFINE(vib_go, 0, 1);
static sensors_vib_sink_t vib_sink;
static size_t vib_room;
static uint8_t vib_axis = 2; /* chip axis along the pod's z (normal to the skin) */
static bool vib_neg;

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
		green_sum += v;
		green_n++;
		/* the filters run at 25 Hz: a 100 or 200 Hz mode feeds them the mean of 4 or 8 */
		algo_acc += v;
		if (++algo_i < algo_k) {
			k_mutex_unlock(&lock);
			return;
		}
		v = (int32_t)(algo_acc / algo_k);
		algo_acc = 0;
		algo_i = 0;
		if (pulse_push(&pulse, v, acc_valid ? acc_now : NULL) && ox_active) {
			struct pulse_out po;

			/* this second's motion and pulse gate the burst windows */
			if (pulse_get(&pulse, &po)) {
				spo2_gate(&ox, po.motion_mg, po.pulse_present);
			}
		}
		if (pulse.enabled) {
			sigq_push(&sq, v, pulse_last_bp(&pulse));
		}
		hrv_push(&hrv, v);
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
	int32_t chip[3], mg[3];

	for (int i = 0; i < 3; i++) {
		chip[i] = (int32_t)(sensor_value_to_milli(&a[i]) * 1000 / 9807);
	}
	/* everything downstream (posture, motion stream, HR, activity) is in the pod frame */
	axes_to_pod(imu_rot, chip, mg);
	for (int i = 0; i < 3; i++) {
		g_sum[i] += mg[i];
		acc_now[i] = (int16_t)CLAMP(mg[i], INT16_MIN, INT16_MAX);
	}
	acc_valid = true;
	g_n++;
	activity_push(&act, mg[0], mg[1], mg[2]);
	activity_push(&act_epoch, mg[0], mg[1], mg[2]);
	/* the step rhythm and the MLC count run at about 12.5 Hz: 104 Hz keeps every 8th */
	bool algo_due = ++imu_i >= imu_k;
	bool mlc_due = false;

	if (algo_due) {
		imu_i = 0;
		norm_ring[norm_head] =
			sqrtf((float)(mg[0] * mg[0] + mg[1] * mg[1] + mg[2] * mg[2])) / 1000.0f;
		norm_head = (uint8_t)((norm_head + 1) % MLC_RHYTHM_N);
		norm_n = (uint8_t)MIN(norm_n + 1, MLC_RHYTHM_N);
		mlc_due = mlc_ok && ++mlc_count >= MLC_WINDOW;
	}

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
	uint8_t k = mode_algo_decim(m);
	uint8_t green_hz = slot >= 0 ? m->hz / k : 0;

	k_mutex_lock(&lock, K_FOREVER);
	algo_k = k;
	algo_i = 0;
	algo_acc = 0;
	green_slot = slot;
	pulse_led = slot >= 0 ? m->slot[slot].led : LED_GREEN;
	green_sum = 0;
	green_n = 0;
	pulse_init(&pulse, green_hz);
	hrv_init(&hrv, green_hz);
	sigq_reset(&sq);
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

static void vib_emit(const uint8_t *pkt, size_t len, void *user)
{
	sensors_vib_sink_t sink = vib_sink;

	ARG_UNUSED(user);
	if (sink) {
		sink(pkt, len);
	}
}

static int vib_write(const struct vib_reg *w, size_t n)
{
	int ret = 0;

	/* every write, even after a failure: leaving must reach the power-down and LP writes */
	for (size_t i = 0; i < n; i++) {
		int r = i2c_reg_write_byte_dt(&imu_i2c, w[i].reg, w[i].val);

		ret = ret ? ret : r;
	}
	return ret;
}

/* Drain the FIFO: a burst whenever the watermark is up, everything once it has filled (FIFO
 * mode then stops), then restart it and flag the gap. */
static void vib_drain(void *p1, void *p2, void *p3)
{
	static uint8_t raw[VIB_BURST_WORDS * VIB_WORD]; /* EasyDMA reads into RAM */

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	for (;;) {
		k_sem_take(&vib_go, K_FOREVER);
		atomic_val_t gen = atomic_get(&vib_gen);
		bool stopped = false;

		while (atomic_get(&vib_on) && atomic_get(&vib_gen) == gen) {
			uint8_t st[2];
			bool idle = true;

			k_mutex_lock(&imu_bus, K_FOREVER);
			if (atomic_get(&vib_on) && atomic_get(&vib_gen) == gen &&
			    i2c_burst_read_dt(&imu_i2c, VIB_REG_FIFO_STATUS1, st, 2) == 0) {
				uint64_t t_us = k_ticks_to_us_floor64(k_uptime_ticks());
				struct vib_status fs = vib_parse_status(st);
				size_t n = MIN(fs.level, VIB_BURST_WORDS);

				stopped |= fs.stopped;
				size_t n_read = fs.wtm || stopped ? n : 0;
				/* a stopped FIFO's words date from when it filled, not from now */
				uint64_t t_new = vib_cap_clock(&vib, &fs, stopped, n_read, t_us);

				if (n_read &&
				    i2c_burst_read_dt(&imu_i2c, VIB_REG_FIFO_OUT, raw, n * VIB_WORD) == 0) {
					vib_cap_burst(&vib, raw, n, fs.level, t_new, vib_emit, NULL);
					idle = false;
				}
				if (stopped && fs.level <= n) {
					struct vib_reg w[2];

					(void)vib_write(w, vib_restart(w));
					vib_cap_gap(&vib, vib_emit, NULL);
					stopped = false;
				}
			}
			k_mutex_unlock(&imu_bus);
			if (idle) {
				k_sleep(K_MSEC(3)); /* 10 or 20 samples */
			}
		}
	}
}

K_THREAD_DEFINE(vib_tid, 1536, vib_drain, NULL, NULL, NULL, 5, 0, 0);

/* Into, out of or between the vibration modes (vibcap.h). Out of one, the Zephyr driver then
 * sets the next mode's rate (imu_apply). */
static int vib_apply(enum pod_mode from, enum pod_mode to)
{
	struct vib_reg w[VIB_MAX_WRITES];
	struct sensor_trigger trig = {.type = SENSOR_TRIG_DATA_READY,
				      .chan = SENSOR_CHAN_ACCEL_XYZ};
	size_t n = vib_transition(from, to, w);

	atomic_set(&vib_on, 0);
	atomic_inc(&vib_gen);
	k_mutex_lock(&imu_bus, K_FOREVER);
	if (mode_is_vibration(to)) {
		(void)sensor_trigger_set(imu, &trig, NULL); /* no data-ready reads at kHz rates */
	}
	int ret = vib_write(w, n);

	if (ret == 0 && mode_is_vibration(to)) {
		vib_cap_start(&vib, to, vib_axis, vib_neg, vib_room);
		atomic_set(&vib_on, 1);
		k_sem_give(&vib_go);
	}
	k_mutex_unlock(&imu_bus);
	return ret;
}

int sensors_init(void)
{
	if (!axes_valid(imu_rot)) {
		LOG_ERR("imu-to-pod is not a rotation");
	}
	/* the pod z row of imu-to-pod names the chip axis normal to the skin, and its sign */
	for (uint8_t i = 0; i < 3; i++) {
		if (imu_rot[6 + i]) {
			vib_axis = i;
			vib_neg = imu_rot[6 + i] < 0;
		}
	}
	activity_init(&act);
	activity_init(&act_epoch);
	pulse_init(&pulse, 50);
	hrv_init(&hrv, 50);
	sigq_init(&sq, &(struct sigq_cfg)SIGQ_CFG_GREEN, &(struct sigq_cfg)SIGQ_CFG_IR);
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
	if (m->imu_mhz != imu_rate_mhz) {
		k_mutex_lock(&lock, K_FOREVER);
		imu_k = mode_imu_decim(m);
		imu_i = 0;
		norm_n = 0; /* the rhythm window holds one rate */
		k_mutex_unlock(&lock);
	}
	imu_rate_mhz = m->imu_mhz;
	if (m->imu_mhz == 0 || mode_is_vibration(mode)) {
		acc_valid = false; /* no motion samples for the pulse filter */
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
	int b = 0;

	/* a vibration mode keeps capturing through re-applies of itself (settings, optics) */
	if (mode != imu_mode && (mode_is_vibration(mode) || mode_is_vibration(imu_mode))) {
		b = vib_apply(imu_mode, mode);
	}
	if (!mode_is_vibration(mode)) {
		int c = imu_apply(m);

		b = b ? b : c;
	}
	imu_mode = mode;

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

void sensors_set_vib_sink(sensors_vib_sink_t sink, size_t room)
{
	k_mutex_lock(&imu_bus, K_FOREVER);
	vib_sink = sink;
	vib_room = room;
	k_mutex_unlock(&imu_bus);
}

void sensors_set_ppg_sink(sensors_ppg_sink_t sink)
{
	ppg_set_callbacks(ppg_sample, sink);
}

bool sensors_pulse_dc(int32_t *dc, uint8_t *led)
{
	bool ok;

	k_mutex_lock(&lock, K_FOREVER);
	*led = pulse_led;
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

void sensors_sigq(struct sigq_out *out)
{
	/* no LED AGC: each mode drives the pulse LED at one fixed current, its maximum */
	struct sigq_in in = {.led_at_max = true};

	k_mutex_lock(&lock, K_FOREVER);
	/* the last second's motion, so "hold still" clears as soon as the pod stops */
	in.motion_mg = pulse_recent_motion_mg(&pulse, PULSE_FS);
	struct pulse_out po;

	in.pulse_present = pulse_get(&pulse, &po) && po.pulse_present;
	in.ir = pulse_led != LED_GREEN;
	sigq_eval(&sq, &in, out);
	k_mutex_unlock(&lock);
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

uint16_t sensors_activity_epoch(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	struct activity_minute m = activity_close(&act_epoch);

	k_mutex_unlock(&lock);
	return m.index_mg;
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
	/* 12.5 Hz, or 104 / 8 = 13 Hz in the research modes */
	uint32_t rhythm_mhz = imu_rate_mhz ? imu_rate_mhz / imu_k : MLC_FS_MHZ;

	r = mlc_rec;
	mlc_record_reset(&mlc_rec);
	n = norm_n;
	for (uint8_t i = 0; i < n; i++) {
		norm[i] = norm_ring[(norm_head + MLC_RHYTHM_N - n + i) % MLC_RHYTHM_N];
	}
	k_mutex_unlock(&lock);

	float spm = 0.0f, share = 0.0f;
	bool ok = n == MLC_RHYTHM_N &&
		  mlc_step_rhythm(norm, n, (float)rhythm_mhz / 1000.0f, &spm, &share);

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
