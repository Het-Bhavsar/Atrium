/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Atrium pod: one-second loop around the power state machine.
 */
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_BOOTLOADER_MCUBOOT)
#include <zephyr/dfu/mcuboot.h>
#endif
#if defined(CONFIG_MCUMGR_GRP_IMG_UPLOAD_CHECK_HOOK)
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt_defines.h>
#endif

#include "app_state.h"
#include "ble.h"
#include "logstore.h"
#include "power_hw.h"
#include "power_sm.h"
#include "sensors.h"
#include "prefs.h"
#include "dfu_health.h"
#include "headroom.h"
#include "ppg.h"
#include "soc_est.h"
#include "spo2.h"
#include "status_led.h"
#include "wear.h"
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(main, CONFIG_ATRIUM_LOG_LEVEL);

#define VBAT_PERIOD_S 10

/* Cell and charger constants are board facts: atrium_pod.dts, zephyr,user */
#define USER DT_PATH(zephyr_user)

static struct power_sm sm;
static struct wear wear;
static struct headroom headroom;
static struct soc soc;
static bool vbat_seen;
static atomic_t ship_request;
static atomic_t settings_dirty;

struct tick_inputs {
	struct power_sm_out out;
	uint32_t vbus_mv;
	bool vbat_valid;
	uint16_t vbat_mv;
	uint8_t soc;
	struct ntc_set ntc;
	struct charge_temps temps;
	bool chrg_n_low;
	uint8_t bpm;
	uint8_t conf;
	bool minute;
	struct activity_minute act;
	bool on_skin;
	bool pulse_ir;
	bool log_due;
	struct hrv_window hrv;
	int16_t pitch_deg;
	int16_t roll_deg;
	struct sensors_motion motion; /* closed with the log record */
	uint8_t pulse_s;              /* seconds with a pulse present in this record */
	bool ox_new;
	struct spo2_out ox;
	uint32_t ox_time;
};

static void settings_changed(void)
{
	atomic_set(&settings_dirty, 1);
}

static int ship_now(void)
{
	if (sm.dock.docked) {
		return -EBUSY;
	}
	atomic_set(&ship_request, 1);
	return 0;
}

static const struct ble_handlers handlers = {
	.settings_changed = settings_changed,
	.ship_now = ship_now,
};

#if defined(CONFIG_MCUMGR_GRP_IMG_UPLOAD_CHECK_HOOK)
/* No firmware upload on a low cell off the dock: a swap must not be cut short by the cut-off. */
static enum mgmt_cb_return dfu_guard(uint32_t event, enum mgmt_cb_return prev, int32_t *rc,
				     uint16_t *group, bool *abort_more, void *data, size_t size)
{
	struct app_status s;

	ARG_UNUSED(event);
	ARG_UNUSED(prev);
	ARG_UNUSED(group);
	ARG_UNUSED(abort_more);
	ARG_UNUSED(data);
	ARG_UNUSED(size);
	app_status_get(&s);
	if (!s.power.docked && s.vbat_mv < CONFIG_ATRIUM_DFU_MIN_MV) {
		*rc = MGMT_ERR_EBUSY;
		return MGMT_CB_ERROR_RC;
	}
	return MGMT_CB_OK;
}

static struct mgmt_callback dfu_cb = {
	.callback = dfu_guard,
	.event_id = MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK,
};
#endif

static void store_status(struct app_status *s, void *arg)
{
	const struct tick_inputs *t = arg;

	s->power = t->out;
	s->vbus_mv = (uint16_t)MIN(t->vbus_mv, UINT16_MAX);
	if (t->vbat_valid) {
		s->vbat_mv = t->vbat_mv;
	}
	s->soc = t->soc;
	s->skin_valid = t->ntc.skin.valid;
	s->skin_cc = (int16_t)t->ntc.skin.cc;
	s->top_valid = t->ntc.top.valid;
	s->top_cc = (int16_t)t->ntc.top.cc;
	s->chrg_n_low = t->chrg_n_low;
	s->bpm = t->bpm;
	s->bpm_conf = t->conf;
	s->on_skin = t->on_skin;
	s->pulse_ir = t->pulse_ir;
	if (t->log_due) {
		s->hrv = t->hrv;
		s->pitch_deg = t->pitch_deg;
		s->roll_deg = t->roll_deg;
		s->mlc_max = t->motion.max;
		s->mlc_class = t->motion.cls;
		s->steps_pm = t->motion.steps_pm;
	}
	if (t->ox_new) {
		s->ox_seq++;
		s->ox_time = t->ox_time;
		s->ox_r = t->ox.r;
		s->ox_conf = t->ox.confidence;
		s->ox_good = t->ox.n_good;
	}
	if (t->minute) {
		s->activity_mg = t->act.index_mg;
		s->moving = t->act.moving;
		s->still = t->act.still;
	}
}

static void log_tick(const struct app_status *s, const struct tick_inputs *t,
		     const struct spo2_out *ox)
{
	struct log_record r = {
		.time = app_time_now(),
		.bpm = s->bpm,
		.bpm_conf = s->bpm_conf,
		.skin_cc = s->skin_cc,
		.top_cc = s->top_cc,
		.activity_mg = s->activity_mg,
		.vbat_mv = s->vbat_mv,
		.soc = s->soc,
		.mode = (uint8_t)s->power.mode,
		.docked = s->power.docked,
		.charging = s->power.chg == CHG_CHARGING || s->power.chg == CHG_COLD_LIMITED,
		.rmssd_x10 = s->hrv.rmssd_x10,
		.rr_diffs = s->hrv.n_diffs,
		.pulse_ir = s->pulse_ir,
		.pitch_deg = s->pitch_deg,
		.roll_deg = s->roll_deg,
		.mlc_max = t->motion.max,
		.mlc_median = t->motion.median,
		.mlc_class = t->motion.cls,
		.steps_pm = t->motion.steps_pm,
		.pulse_s = t->pulse_s,
		/* a burst that ended during this record */
		.ox_conf = ox ? ox->confidence : 0.0f,
		.ox_r = ox && !isnan(ox->r) ? ox->r : 0.0f,
	};

	if (logstore_append(&r)) {
		LOG_WRN("log append failed");
	}
}

int main(void)
{
	struct power_sm_cfg cfg = POWER_SM_CFG_DEFAULT;
	struct tick_inputs t = {0};
	enum pod_mode applied = MODE_COUNT;
	bool inhibit_applied = false;
	uint32_t tick = 0;
	int ret;

	struct wear_cfg wcfg = WEAR_CFG_DEFAULT;
	struct soc_cfg scfg = SOC_CFG_DEFAULT;
	/* typical ICHG: K_ISET 135 against the worst-case 145 in the devicetree */
	const uint32_t ichg_typ_ua = DT_PROP(USER, charge_current_max_ua) * 135U / 145U;
	bool chrg_was_low = false;

	cfg.charge.update_ms = CONFIG_ATRIUM_TICK_MS;
	cfg.charge.cell_mah = DT_PROP(USER, cell_capacity_mah);
	/* below its TS half-current threshold the BQ25101 gives half of ICHG */
	cfg.charge.chg_cold_ua = DT_PROP(USER, charge_current_max_ua) / 2;
	cfg.cutoff.cutoff_mv = DT_PROP(USER, cutoff_mv);
	cfg.cutoff.low_mv = cfg.cutoff.cutoff_mv + 150;
	cfg.cutoff.low_clear_mv = cfg.cutoff.cutoff_mv + 250;
	power_sm_init(&sm, &cfg);
	wear_init(&wear, &wcfg);
	scfg.cell_mah = DT_PROP(USER, cell_capacity_mah);
	scfg.cell_mohm = DT_PROP(USER, cell_resistance_mohm);
	soc_init(&soc, &scfg);
	/* green limit includes the cell's sag at the largest green current in the mode table */
	uint8_t green_ma_max = 0;

	for (int i = 0; i < MODE_COUNT; i++) {
		const struct mode_cfg *m = mode_get(i);

		for (uint8_t s = 0; s < m->n_slots; s++) {
			if (m->slot[s].led == LED_GREEN && m->slot[s].ma > green_ma_max) {
				green_ma_max = m->slot[s].ma;
			}
		}
	}
	headroom_init(&headroom, headroom_limit_mv(DT_PROP(USER, green_min_mv), green_ma_max,
						   DT_PROP(USER, cell_resistance_mohm)));
	bool green_ok = true;
	bool optics_on = true;
	bool ble_ok;

	ret = power_hw_init();
	if (ret) {
		LOG_ERR("power hw init %d", ret);
	}
	(void)status_led_init();
	status_led_flash(3); /* alive, for bring-up */
	(void)sensors_init();
	ret = ble_init(&handlers);
	ble_ok = ret == 0;
	if (ret) {
		LOG_ERR("bt_enable %d", ret);
	}
	(void)prefs_init();
	(void)logstore_init();
	(void)ble_start();
#if defined(CONFIG_MCUMGR_GRP_IMG_UPLOAD_CHECK_HOOK)
	mgmt_callback_register(&dfu_cb);
#endif

	int64_t next = k_uptime_get();
	struct spo2_sched ox_sched;
	struct spo2_out ox_rec; /* the latest burst, held for the current log record */
	bool ox_in_rec = false;

	spo2_sched_init(&ox_sched, (uint32_t)(next / 1000));

	for (;; tick++) {
		uint32_t now = k_uptime_get_32();
		struct app_settings set;
		struct power_sm_in in = {.now_ms = now};
		const struct mode_cfg *mc = mode_get(applied < MODE_COUNT ? applied : MODE_SMART);
		bool temp_due = (tick % mc->tmp_period_s) == 0 || sm.dock.docked;

		app_settings_get(&set);

		in.vbus_sense_valid = power_hw_vbus_sense_mv(&in.vbus_sense_mv) == 0;
		t.vbus_mv = dock_vbus_mv(&cfg.dock, in.vbus_sense_mv);

		if (temp_due) {
			power_hw_ntcs(&t.ntc, now);
		}
		/* the charge policy's cell proxy is TH4 (top) alone: the r5 board has no TH3, and
		 * the cell's own NTC TH2 on the BQ25101 TS pin is the hardware backstop */
		t.temps.primary = t.ntc.top;
		t.temps.backup = (struct temp_reading){0};
		in.temps = t.temps;

		/* VBAT and the state-of-charge estimate (no fuel gauge on r5) */
		t.vbat_valid = false;
		if (tick % VBAT_PERIOD_S == 0) {
			t.vbat_valid = power_hw_vbat_mv(&t.vbat_mv) == 0;
			vbat_seen |= t.vbat_valid;
			if (t.vbat_valid) {
				soc_on_vbat(&soc, t.vbat_mv, mc->budget_typ_ua);
			}
		}
		bool green_was = green_ok;

		if (t.vbat_valid) {
			green_ok = headroom_update(&headroom, t.vbat_mv);
		}
		in.vbat_valid = t.vbat_valid;
		in.vbat_mv = t.vbat_mv;
		t.chrg_n_low = power_hw_chrg_n_low();
		in.chrg_n_low = t.chrg_n_low;
		in.user_mode = (enum pod_mode)set.mode;
		in.night = app_is_night(&set);

		/* wear detection on last second's green level and pulse */
		int32_t dc;
		bool dc_ok = sensors_green_dc(&dc);
		uint8_t green_ma = 0;

		struct pulse_out po;
		bool po_ok = sensors_pulse(&po);

		for (uint8_t i = 0; i < mc->n_slots; i++) {
			if (mc->slot[i].led == LED_GREEN) {
				green_ma = mc->slot[i].ma;
				break;
			}
		}
		if (sm.dock.docked) {
			wear_restart(&wear); /* lifted from the dock: measure, learn, then decide */
		} else if (green_ma) {
			(void)wear_update(&wear, dc_ok, (float)dc / green_ma, po_ok && po.pulse_present,
					  po_ok ? po.motion_mg : 0.0f, t.ntc.skin.valid,
					  (float)t.ntc.skin.cc / 100.0f);
		}
		in.worn = wear.worn;

		power_sm_step(&sm, &in, &t.out);

		/* charge flow for the estimate; the charger releasing CHG# ends a cycle: full */
		uint32_t in_ua = 0;

		if (t.out.chg == CHG_CHARGING) {
			in_ua = ichg_typ_ua;
		} else if (t.out.chg == CHG_COLD_LIMITED && !t.out.chg_inhibit) {
			in_ua = ichg_typ_ua / 2;
		}
		soc_flow(&soc, CONFIG_ATRIUM_TICK_MS, mc->budget_typ_ua, in_ua);
		if (chrg_was_low && !t.chrg_n_low && t.out.docked && t.out.band == CHG_BAND_NORMAL) {
			soc_full(&soc);
		}
		chrg_was_low = t.chrg_n_low;
		t.soc = soc_percent(&soc);

		if (t.out.chg_inhibit != inhibit_applied) {
			inhibit_applied = t.out.chg_inhibit;
			(void)power_hw_chg_inhibit(inhibit_applied);
			LOG_INF("charge %s (%s band)", inhibit_applied ? "inhibited" : "released",
				charge_band_name(t.out.band));
		}

		if ((t.out.ship || atomic_get(&ship_request)) && !t.out.docked) {
			atomic_set(&ship_request, 0);
			(void)sensors_set_mode(MODE_DOCKED_IDLE, green_ok, false);
			(void)power_hw_ship();
			/* still alive: a charger appeared during the hold */
			applied = MODE_COUNT;
		}

		bool optics_was = optics_on;

		optics_on = mode_optics_on(mode_get(t.out.mode), tick);
		if (t.out.mode != applied || green_ok != green_was || optics_on != optics_was ||
		    atomic_cas(&settings_dirty, 1, 0)) {
			applied = t.out.mode;
			(void)sensors_set_mode(applied, green_ok, optics_on);
			ble_set_mode(applied);
		}

		t.on_skin = t.out.state == POD_WORN;
		/* the pulse is reported only while worn and the track is valid; off the body the
		 * tracker can follow a noise peak (pulse presence gates wear, not this) */
		t.bpm = 0;
		t.conf = 0;
		if (po_ok && t.on_skin && po.valid && !isnan(po.bpm)) {
			t.bpm = (uint8_t)CLAMP(lroundf(po.bpm), 0, 255);
			t.conf = (uint8_t)CLAMP(lroundf(po.conf * 255.0f), 0, 255);
		}
		t.pulse_ir = !green_ok;

		/* MLC levels feed the burst schedule; a burst needs the pod worn and still, with a
		 * pulse, green headroom, and its optics on */
		uint8_t lv;
		uint32_t now_s = now / 1000U;

		if (sensors_mlc_poll(&lv)) {
			spo2_sched_level(&ox_sched, now_s, lv);
		}
		bool pulse_now = po_ok && po.pulse_present;
		bool ox_can = t.on_skin && green_ok && optics_on && !sensors_spo2_active();

		if (spo2_sched_tick(&ox_sched, now_s, applied, ox_can, pulse_now)) {
			ret = sensors_spo2_start();
			LOG_INF("oxygen burst %d", ret);
		}
		t.ox_new = sensors_spo2_poll(&t.ox);
		if (t.ox_new) {
			t.ox_time = app_time_now();
			ox_rec = t.ox;
			ox_in_rec = true;
		}
		if (pulse_now && t.on_skin && t.pulse_s < 15) {
			t.pulse_s++;
		}

		t.log_due = tick % set.log_interval_s == 0;
		if (t.log_due) {
			t.hrv = sensors_hrv_close();
			sensors_posture_close(&t.pitch_deg, &t.roll_deg);
			t.motion = sensors_motion_close();
		}
		t.minute = tick % 60 == 59;
		if (t.minute) {
			t.act = sensors_activity_minute();
		}
		app_status_update(store_status, &t);

		struct app_status s;

		app_status_get(&s);
		if (t.log_due) {
			log_tick(&s, &t, ox_in_rec ? &ox_rec : NULL);
			ox_in_rec = false;
			t.pulse_s = 0;
		}

		/* status LED: a flash every 4 s while charging, two when temperature blocks it */
		if (t.out.docked && tick % 4 == 0) {
			if (t.out.chg == CHG_CHARGING || t.out.chg == CHG_COLD_LIMITED) {
				status_led_flash(1);
			} else if (t.out.chg == CHG_TEMP_BLOCKED) {
				status_led_flash(2);
			}
		}
		ble_tick(&s, t.minute, temp_due);

#if defined(CONFIG_BOOTLOADER_MCUBOOT)
		switch (dfu_health_decide(boot_is_img_confirmed(), tick, ble_ok && ppg_ready() &&
					  sensors_imu_ready() && vbat_seen)) {
		case DFU_CONFIRM:
			(void)boot_write_img_confirmed();
			LOG_INF("image confirmed");
			break;
		case DFU_REVERT:
			LOG_ERR("new image unhealthy: reboot to the previous one");
			sys_reboot(SYS_REBOOT_COLD);
			break;
		default:
			break;
		}
#endif
		next += CONFIG_ATRIUM_TICK_MS;
		k_sleep(K_TIMEOUT_ABS_MS(next));
	}
	return 0;
}
