/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Atrium pod: one-second loop around the power state machine.
 */
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#if defined(CONFIG_BOOTLOADER_MCUBOOT)
#include <zephyr/dfu/mcuboot.h>
#endif
#if defined(CONFIG_MCUMGR_GRP_IMG_UPLOAD_CHECK_HOOK)
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt_defines.h>
#endif

#include "app_state.h"
#include "ble.h"
#include "charger.h"
#include "dfu_health.h"
#include "epoch.h"
#include "headroom.h"
#include "logstore.h"
#include "power_hw.h"
#include "power_sm.h"
#include "ppg.h"
#include "prefs.h"
#include "sensors.h"
#include "soc_est.h"
#include "spo2.h"
#include "wear.h"

LOG_MODULE_REGISTER(main, CONFIG_ATRIUM_LOG_LEVEL);

#define VSYS_PERIOD_S    10
/* the BQ21061 I2C watchdog is 50 s: service it well inside that */
#define CHARGER_PERIOD_S 10
/* connectable for new pairings this long after a lift from the Plinth, and after boot */
#define PAIR_WINDOW_S    60

/* Cell and supply constants are board facts: atrium_pod.dts */
#define USER DT_PATH(zephyr_user)
#define CHG  DT_ALIAS(charger0)

static struct power_sm sm;
static struct wear wear;
static struct headroom headroom;
static struct soc soc;
static bool vsys_seen;
static atomic_t ship_request;
static atomic_t settings_dirty;

struct tick_inputs {
	struct power_sm_out out;
	bool vsys_valid;
	uint16_t vsys_mv;
	uint8_t soc;
	struct ntc_set ntc;
	struct chg_report chg;
	uint8_t stat0, stat1;
	uint8_t bpm;
	uint8_t conf;
	uint8_t live_bpm;
	bool minute;
	struct activity_minute act;
	bool on_skin;
	bool on_case;
	bool pulse_ir;
	bool epoch_due;
	struct epoch_close_in ec;
	bool ox_new;
	struct spo2_out ox;
	uint32_t ox_time;
	struct sigq_out sq;
};

static void settings_changed(void)
{
	atomic_set(&settings_dirty, 1);
}

/* On the Plinth: ship mode at the next lift. Off it: power off until the Plinth. */
static int ship_now(void)
{
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
	if (!s.power.docked && s.vsys_mv < CONFIG_ATRIUM_DFU_MIN_MV) {
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
	if (t->vsys_valid) {
		s->vsys_mv = t->vsys_mv;
	}
	s->soc = t->soc;
	s->skin_valid = t->ntc.skin.valid;
	s->skin_cc = (int16_t)t->ntc.skin.cc;
	s->top_valid = t->ntc.top.valid;
	s->top_cc = (int16_t)t->ntc.top.cc;
	s->chg_stat0 = t->stat0;
	s->chg_stat1 = t->stat1;
	s->bpm = t->bpm;
	s->bpm_conf = t->conf;
	s->live_bpm = t->live_bpm;
	s->on_skin = t->on_skin;
	s->on_case = t->on_case;
	s->pulse_ir = t->pulse_ir;
	s->sq = t->sq;
	if (t->epoch_due) {
		s->hrv = t->ec.hrv;
		s->pitch_deg = t->ec.pitch_deg;
		s->roll_deg = t->ec.roll_deg;
		s->mlc_max = t->ec.mlc_max;
		s->mlc_class = t->ec.cls;
		s->steps_pm = t->ec.steps_pm;
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

/* LED current of the pulse LED in the applied mode, after any green-to-IR swap */
static uint8_t pulse_led_ma(const struct mode_cfg *mc, bool green_ok, uint8_t led)
{
	struct mode_cfg low;

	if (!green_ok) {
		(void)mode_green_to_ir(mc, &low);
		mc = &low;
	}
	for (uint8_t i = 0; i < mc->n_slots; i++) {
		if (mc->slot[i].led == led) {
			return mc->slot[i].ma;
		}
	}
	return 0;
}

static void power_down(void)
{
	(void)logstore_close();
	(void)sensors_set_mode(MODE_DOCKED_IDLE, true, false);
	power_hw_poweroff();
}

int main(void)
{
	struct power_sm_cfg cfg = POWER_SM_CFG_DEFAULT;
	struct tick_inputs t = {0};
	enum pod_mode applied = MODE_COUNT;
	uint32_t tick = 0;
	bool was_docked = false;
	bool was_done = false;
	bool chg_cold_set = false;
	bool chg_disable_set = false;
	int ret;

	struct wear_cfg wcfg = WEAR_CFG_DEFAULT;
	struct soc_cfg scfg = SOC_CFG_DEFAULT;
	/* ICHG accuracy is a few percent at 15 mA [VERIFY]: the estimate uses the set value */
	const uint32_t ichg_ua = DT_PROP(CHG, constant_charge_current_max_microamp);
	const uint32_t ichg_cold_ua = DT_PROP(CHG, cold_charge_current_microamp);

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
	ret = charger_init();
	if (ret) {
		LOG_ERR("charger init %d", ret);
	}
	(void)sensors_init();
	ret = ble_init(&handlers);
	ble_ok = ret == 0;
	if (ret) {
		LOG_ERR("bt_enable %d", ret);
	}
	(void)prefs_init();
	ret = logstore_init();
	if (ret) {
		LOG_ERR("log %d", ret);
	}
	(void)ble_start();
	ble_pairing_window(PAIR_WINDOW_S);
#if defined(CONFIG_MCUMGR_GRP_IMG_UPLOAD_CHECK_HOOK)
	mgmt_callback_register(&dfu_cb);
#endif

	int64_t next = k_uptime_get();
	struct spo2_sched ox_sched;
	struct spo2_out ox_rec; /* the latest burst, held for the current epoch */
	bool ox_in_rec = false;
	struct epoch ep = {0};
	bool ep_prev = false;

	spo2_sched_init(&ox_sched, (uint32_t)(next / 1000));

	for (;; tick++) {
		uint32_t now = k_uptime_get_32();
		struct app_settings set;
		struct power_sm_in in = {.now_ms = now};
		const struct mode_cfg *mc = mode_get(applied < MODE_COUNT ? applied : MODE_SMART);
		bool temp_due = (tick % mc->tmp_period_s) == 0 || sm.dock.docked;

		app_settings_get(&set);
		in.pg_low = charger_pg_low();
		if (tick % LOG_EPOCH_S == 0) {
			epoch_open(&ep, epoch_next_t0(ep.t0, ep_prev, app_time_now()));
			ep_prev = true;
		}

		if (temp_due) {
			power_hw_ntcs(&t.ntc, now);
		}
		/* the charge policy's sensor is TH4 alone; the charger's TS (TH5, on the cell) is
		 * the first line */
		in.temps.primary = t.ntc.top;
		in.temps.backup = (struct temp_reading){0};

		/* VSYS: the cell off the Plinth, 4.5 V regulated on it (no fuel gauge) */
		t.vsys_valid = false;
		if (tick % VSYS_PERIOD_S == 0) {
			t.vsys_valid = power_hw_vsys_mv(&t.vsys_mv) == 0;
			if (t.vsys_valid && !sm.dock.docked) {
				vsys_seen = true;
				soc_on_vbat(&soc, t.vsys_mv, mc->budget_typ_ua);
			}
		}
		bool green_was = green_ok;

		if (sm.dock.docked) {
			green_ok = true; /* VLED = VSYS = 4.5 V */
		} else if (t.vsys_valid) {
			green_ok = headroom_update(&headroom, t.vsys_mv);
		}
		in.vsys_valid = t.vsys_valid;
		in.vsys_mv = t.vsys_mv;
		in.charger = t.chg;
		in.user_mode = gatt_atrium_user_mode(set.mode);
		in.green_low = !green_ok;
		in.night = app_is_night(&set);

		/* wear detection on last second's pulse-LED level and pulse */
		int32_t dc;
		uint8_t led;
		bool dc_ok = sensors_pulse_dc(&dc, &led);
		struct pulse_out po;
		bool po_ok = sensors_pulse(&po);
		uint8_t led_ma = pulse_led_ma(mc, green_ok, led);

		if (sm.dock.docked) {
			wear_restart(&wear); /* lifted from the dock: measure, learn, then decide */
		} else if (led_ma) {
			(void)wear_update(&wear, led, dc_ok, (float)dc / led_ma,
					  po_ok && po.pulse_present, po_ok ? po.motion_mg : 0.0f,
					  t.ntc.skin.valid, (float)t.ntc.skin.cc / 100.0f);
		}
		in.worn = wear.worn;
		in.fit = gatt_atrium_fit();

		power_sm_step(&sm, &in, &t.out);

		/* the charger: on every landing, after every VIN drop-out (the case's pulses; the
		 * BQ21061 may come back with its reset ICHG), when the policy changes, and every
		 * CHARGER_PERIOD_S while docked. Only with /PG low: mid-pulse it does not answer. */
		bool chg_changed = t.out.chg_cold != chg_cold_set || t.out.chg_disable != chg_disable_set;
		bool vin_edge = in.pg_low && charger_vin_good_edge();

		if (t.out.docked && in.pg_low &&
		    (!was_docked || chg_changed || vin_edge || tick % CHARGER_PERIOD_S == 0)) {
			ret = charger_service(t.out.chg_cold, t.out.chg_disable, &t.chg, &t.stat0,
					      &t.stat1);
			if (ret) {
				LOG_WRN("charger %d", ret);
			}
			chg_cold_set = t.out.chg_cold;
			chg_disable_set = t.out.chg_disable;
			/* this second's state from this second's charger status */
			t.out.chg = chg_state_classify(true, t.out.band, &t.chg);
		} else if (!t.out.docked) {
			t.chg = (struct chg_report){0};
			t.stat0 = t.stat1 = 0;
		}
		if (t.out.docked != was_docked) {
			LOG_INF("%s", t.out.docked ? "on the Plinth" : "lifted");
			if (t.out.docked) {
				gatt_atrium_research_end(); /* never resumes after a lift */
			} else {
				ble_pairing_window(PAIR_WINDOW_S);
			}
		}

		/* charge flow for the estimate; the charger reporting done means full */
		uint32_t in_ua = t.out.chg == CHG_CHARGING	   ? ichg_ua
				 : t.out.chg == CHG_COLD_LIMITED ? ichg_cold_ua
								   : 0;

		soc_flow(&soc, CONFIG_ATRIUM_TICK_MS, mc->budget_typ_ua, in_ua);
		if (t.chg.done && !was_done) {
			soc_full(&soc);
		}
		was_done = t.chg.done;
		t.soc = soc_percent(&soc);

		if (atomic_cas(&ship_request, 1, 0)) {
			if (t.out.docked) {
				ret = charger_ship_on_lift();
				LOG_INF("ship mode at the next lift (%d)", ret);
			} else {
				power_down();
			}
		}
		if (t.out.poweroff) {
			power_down();
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
		t.on_case = t.out.docked && charger_on_case();
		/* the pulse is reported only while worn and the track is valid; off the body the
		 * tracker can follow a noise peak (pulse presence gates wear, not this) */
		t.bpm = po_ok ? pulse_report_bpm(&po, t.on_skin, false) : 0;
		t.conf = t.bpm ? (uint8_t)CLAMP(lroundf(po.conf * 255.0f), 0, 255) : 0;
		/* the fit guide shows the pulse before wear says worn */
		t.live_bpm = po_ok ? pulse_report_bpm(&po, t.on_skin, in.fit) : 0;
		t.pulse_ir = led != LED_GREEN;
		sensors_sigq(&t.sq);

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

		struct epoch_second es = {
			.docked = t.out.docked,
			.charging = t.out.chg == CHG_CHARGING || t.out.chg == CHG_COLD_LIMITED,
			.on_skin = t.on_skin,
			.pulse_ir = t.pulse_ir,
			.pulse_present = pulse_now,
			.bpm = t.bpm,
			.conf = t.conf / 255.0f,
		};

		epoch_second(&ep, &es);
		t.epoch_due = tick % LOG_EPOCH_S == LOG_EPOCH_S - 1;
		if (t.epoch_due) {
			struct sensors_motion mo = sensors_motion_close();
			struct log_record r;

			t.ec.hrv = sensors_hrv_close();
			sensors_posture_close(&t.ec.pitch_deg, &t.ec.roll_deg);
			t.ec.mlc_max = mo.max;
			t.ec.cls = mo.cls;
			t.ec.steps_pm = mo.steps_pm;
			t.ec.activity_mg = sensors_activity_epoch();
			t.ec.skin_valid = t.ntc.skin.valid;
			t.ec.skin_cc = t.ntc.skin.cc;
			t.ec.ox = ox_in_rec && !isnan(ox_rec.r);
			t.ec.ox_r = t.ec.ox ? ox_rec.r : 0.0f;
			t.ec.ox_conf = t.ec.ox ? ox_rec.confidence : 0.0f;
			ox_in_rec = false;
			bool logged = epoch_close(&ep, &t.ec, &r);

			ret = logged ? logstore_append(ep.t0, &r) : logstore_close();
			if (ret) {
				LOG_WRN("log %d", ret);
			} else if (logged) {
				gatt_atrium_live_epoch(ep.t0, &r);
			}
		}
		t.minute = tick % 60 == 59;
		if (t.minute) {
			t.act = sensors_activity_minute();
		}
		app_status_update(store_status, &t);

		struct app_status s;

		app_status_get(&s);
		ble_tick(&s, t.minute, temp_due);
		was_docked = t.out.docked;

#if defined(CONFIG_BOOTLOADER_MCUBOOT)
		switch (dfu_health_decide(boot_is_img_confirmed(), tick, ble_ok && ppg_ready() &&
					  sensors_imu_ready() && (vsys_seen || t.out.docked))) {
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
