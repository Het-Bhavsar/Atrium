/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Atrium GATT services, docs/ble_gatt.md sections 3 to 8.
 */
#include <string.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>

#include "ble.h"
#include "gatt_encode.h"
#include "logstore.h"
#include "logxfer.h"
#include "ppg.h"
#include "pulse.h"
#include "sensors.h"
#include "prefs.h"

LOG_MODULE_REGISTER(gatt_atrium, CONFIG_ATRIUM_LOG_LEVEL);

#define ATRIUM_UUID(n) BT_UUID_128_ENCODE(0x8f2a0000 | (n), 0x4c46, 0x4154, 0x5249, 0x554d2e706f64)
#define DECL(name, n)  static const struct bt_uuid_128 name = BT_UUID_INIT_128(ATRIUM_UUID(n))

DECL(uuid_power, 0x0100);
DECL(uuid_charge, 0x0101);
DECL(uuid_ppg, 0x0200);
DECL(uuid_ppg_ctrl, 0x0201);
DECL(uuid_ppg_data, 0x0202);
DECL(uuid_pulse, 0x0203);
DECL(uuid_motion, 0x0204);
DECL(uuid_oxygen, 0x0205);
DECL(uuid_fitq, 0x0206);
DECL(uuid_vib, 0x0207);
DECL(uuid_temp_svc, 0x0300);
DECL(uuid_temp, 0x0301);
DECL(uuid_act_svc, 0x0400);
DECL(uuid_act, 0x0401);
DECL(uuid_log, 0x0500);
DECL(uuid_log_info, 0x0501);
DECL(uuid_log_ctrl, 0x0502);
DECL(uuid_log_data, 0x0503);
DECL(uuid_live_epoch, 0x0504);
DECL(uuid_set, 0x0600);
DECL(uuid_mode, 0x0601);
DECL(uuid_time, 0x0602);
DECL(uuid_sleep, 0x0603);
DECL(uuid_logint, 0x0604);
DECL(uuid_cmd, 0x0605);

#define R_ENC  BT_GATT_PERM_READ_ENCRYPT
#define W_ENC  BT_GATT_PERM_WRITE_ENCRYPT
#define CCC_P  (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT)

#define CMD_KEY       0x41545249U
#define ERR_SHIP      0x80

/* defined further down by BT_GATT_SERVICE_DEFINE */
extern const struct bt_gatt_service_static ppg_svc;
extern const struct bt_gatt_service_static log_svc;

static const struct ble_handlers *handlers;
static bool ppg_stream;
static bool motion_stream;
static uint16_t ppg_seq;
static uint16_t motion_seq;
static uint8_t pulse_detail[5];
/* fit mode (PPG Control bit 2): green at 25 Hz before wear says worn, for the fit guide */
#define FIT_MAX_MS (180U * 1000U)
static atomic_t fit_on;
static uint32_t fit_t0;
/* research mode (6 to 11) for this connection only: never stored, so a reboot, a disconnect or
 * the Plinth ends it, and the stored choice comes back */
static atomic_t research;
/* Vibration Data notifications queued and not yet sent: the drain thread must never block on
 * the BLE buffers, so past VIB_INFLIGHT_MAX a packet is dropped (the sample index shows it).
 * 2 of the 3 ATT and ACL TX buffers (CONFIG_BT_ATT_TX_COUNT, BT_BUF_ACL_TX_COUNT), so the
 * other notifications keep one: 2 x 244 B per connection event, 32.5 kB/s at 15 ms */
#define VIB_INFLIGHT_MAX 2
static atomic_t vib_inflight;

/* ---- 3 Power ---- */

static ssize_t read_charge(struct bt_conn *c, const struct bt_gatt_attr *a, void *buf,
			   uint16_t len, uint16_t off)
{
	struct app_status s;
	uint8_t v[GATT_CHARGE_STATE_LEN];

	app_status_get(&s);
	gatt_encode_charge_state(&s, v);
	return bt_gatt_attr_read(c, a, buf, len, off, v, sizeof(v));
}

BT_GATT_SERVICE_DEFINE(power_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_power),
	BT_GATT_CHARACTERISTIC(&uuid_charge.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, R_ENC,
			       read_charge, NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
);

/* ---- 4.2 PPG ---- */

static void ppg_sink(const uint8_t *raw, size_t len)
{
	struct bt_conn *c = ble_conn();
	uint8_t pkt[244];
	struct app_status s;

	if (!c || !ppg_stream) {
		return;
	}
	app_status_get(&s);
	size_t word = ppg_raw_word_bytes();
	size_t room = (MIN(bt_gatt_get_mtu(c) - 3U, sizeof(pkt)) - 8U) / word * word;
	const struct mode_cfg *m = mode_get(s.power.mode);
	uint32_t hz = m && m->hz ? m->hz : 1;
	uint32_t per_sample = (m && m->n_slots ? m->n_slots : 1) * word;
	/* the batch has just been read: its last sample is now [ASSUMED within one sample] */
	uint32_t t0 = k_uptime_get_32() - (uint32_t)((len / per_sample - 1) * 1000U / hz);

	while (len > 0) {
		size_t k = MIN(len, room);

		sys_put_le16(ppg_seq++, pkt);
		pkt[2] = (uint8_t)hz;
		pkt[3] = (uint8_t)s.power.mode;
		sys_put_le32(t0, &pkt[4]);
		memcpy(&pkt[8], raw, k);
		if (bt_gatt_notify(c, &ppg_svc.attrs[4], pkt, (uint16_t)(8 + k))) {
			break; /* no buffers: drop the rest, the sequence number shows the gap */
		}
		t0 += (uint32_t)(k / per_sample * 1000U / hz);
		raw += k;
		len -= k;
	}
}

static void motion_sink(const int16_t (*xyz)[3], size_t n, uint32_t t_first_ms, uint32_t rate_mhz)
{
	struct bt_conn *c = ble_conn();
	uint8_t pkt[8 + SENSORS_MOTION_BATCH * 6];

	if (!c || !motion_stream || bt_gatt_get_mtu(c) - 3U < 8U + n * 6U) {
		return;
	}
	sys_put_le16(motion_seq++, pkt);
	sys_put_le16(gatt_motion_rate(rate_mhz), &pkt[2]);
	sys_put_le32(t_first_ms, &pkt[4]);
	for (size_t i = 0; i < n; i++) {
		for (int k = 0; k < 3; k++) {
			sys_put_le16((uint16_t)xyz[i][k], &pkt[8 + i * 6 + k * 2]);
		}
	}
	(void)bt_gatt_notify(c, &ppg_svc.attrs[10], pkt, (uint16_t)(8 + n * 6));
}

static void vib_sent(struct bt_conn *c, void *user)
{
	ARG_UNUSED(c);
	ARG_UNUSED(user);
	(void)atomic_dec(&vib_inflight);
}

/* 4.4 Vibration Data, from the drain thread */
static void vib_sink(const uint8_t *pkt, size_t len)
{
	struct bt_conn *c = ble_conn();
	struct bt_gatt_notify_params p = {
		.attr = &ppg_svc.attrs[19],
		.data = pkt,
		.len = (uint16_t)len,
		.func = vib_sent,
	};

	if (!c || atomic_get(&vib_inflight) >= VIB_INFLIGHT_MAX) {
		return;
	}
	(void)atomic_inc(&vib_inflight);
	if (bt_gatt_notify_cb(c, &p)) {
		(void)atomic_dec(&vib_inflight);
	}
}

static ssize_t write_ppg_ctrl(struct bt_conn *c, const struct bt_gatt_attr *a, const void *buf,
			      uint16_t len, uint16_t off, uint8_t flags)
{
	ARG_UNUSED(c);
	ARG_UNUSED(a);
	ARG_UNUSED(flags);
	if (off || len != 1 || ((const uint8_t *)buf)[0] > 7) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	/* bit 0: raw PPG stream, bit 1: raw accelerometer stream, bit 2: fit mode */
	uint8_t v = ((const uint8_t *)buf)[0];

	ppg_stream = v & 0x01;
	motion_stream = v & 0x02;
	if (v & 0x04) {
		fit_t0 = k_uptime_get_32();
	}
	atomic_set(&fit_on, (v & 0x04) ? 1 : 0);
	sensors_set_ppg_sink(ppg_stream ? ppg_sink : NULL);
	sensors_set_motion_sink(motion_stream ? motion_sink : NULL);
	return len;
}

static ssize_t read_pulse(struct bt_conn *c, const struct bt_gatt_attr *a, void *buf,
			  uint16_t len, uint16_t off)
{
	return bt_gatt_attr_read(c, a, buf, len, off, pulse_detail, sizeof(pulse_detail));
}

/* last oxygen-trend burst: relative red/IR ratio, research only */
static uint8_t oxygen[GATT_OXYGEN_LEN];
static uint16_t oxygen_seq;

static ssize_t read_oxygen(struct bt_conn *c, const struct bt_gatt_attr *a, void *buf,
			   uint16_t len, uint16_t off)
{
	return bt_gatt_attr_read(c, a, buf, len, off, oxygen, sizeof(oxygen));
}

/* fit quality, encoded every second */
static uint8_t fitq_val[GATT_FIT_QUALITY_LEN] = {SIGQ_NONE};

static ssize_t read_fitq(struct bt_conn *c, const struct bt_gatt_attr *a, void *buf,
			 uint16_t len, uint16_t off)
{
	return bt_gatt_attr_read(c, a, buf, len, off, fitq_val, sizeof(fitq_val));
}

bool gatt_atrium_fit(void)
{
	if (atomic_get(&fit_on) && (uint32_t)(k_uptime_get_32() - fit_t0) >= FIT_MAX_MS) {
		atomic_set(&fit_on, 0);
	}
	return atomic_get(&fit_on) != 0;
}

BT_GATT_SERVICE_DEFINE(ppg_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_ppg),
	BT_GATT_CHARACTERISTIC(&uuid_ppg_ctrl.uuid, BT_GATT_CHRC_WRITE, W_ENC, NULL,
			       write_ppg_ctrl, NULL),
	BT_GATT_CHARACTERISTIC(&uuid_ppg_data.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL,
			       NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
	BT_GATT_CHARACTERISTIC(&uuid_pulse.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, R_ENC,
			       read_pulse, NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
	BT_GATT_CHARACTERISTIC(&uuid_motion.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL,
			       NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
	BT_GATT_CHARACTERISTIC(&uuid_oxygen.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, R_ENC,
			       read_oxygen, NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
	BT_GATT_CHARACTERISTIC(&uuid_fitq.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, R_ENC,
			       read_fitq, NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
	/* attrs[19]: vib_sink */
	BT_GATT_CHARACTERISTIC(&uuid_vib.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL,
			       NULL),
	BT_GATT_CCC(NULL, CCC_P),
);

/* ---- 5 Skin temperature, 6 Activity ---- */

static ssize_t read_temp(struct bt_conn *c, const struct bt_gatt_attr *a, void *buf,
			 uint16_t len, uint16_t off)
{
	struct app_status s;
	uint8_t v[GATT_SKIN_TEMP_LEN];

	app_status_get(&s);
	gatt_encode_skin_temp(&s, app_time_now(), v);
	return bt_gatt_attr_read(c, a, buf, len, off, v, sizeof(v));
}

static ssize_t read_act(struct bt_conn *c, const struct bt_gatt_attr *a, void *buf, uint16_t len,
			uint16_t off)
{
	struct app_status s;
	uint8_t v[GATT_ACTIVITY_LEN];

	app_status_get(&s);
	gatt_encode_activity(&s, app_time_now(), v);
	return bt_gatt_attr_read(c, a, buf, len, off, v, sizeof(v));
}

BT_GATT_SERVICE_DEFINE(temp_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_temp_svc),
	BT_GATT_CHARACTERISTIC(&uuid_temp.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, R_ENC,
			       read_temp, NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
);

BT_GATT_SERVICE_DEFINE(act_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_act_svc),
	BT_GATT_CHARACTERISTIC(&uuid_act.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, R_ENC,
			       read_act, NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
);

/* ---- 7 Log download ---- */

static void src_range(void *ctx, uint32_t *oldest, uint32_t *head)
{
	ARG_UNUSED(ctx);
	logstore_range(oldest, head);
}

static void src_snapshot(void *ctx, struct logpack_snap *s)
{
	ARG_UNUSED(ctx);
	logstore_snapshot(s);
}

static int src_read(void *ctx, const struct logpack_snap *s, uint32_t page, uint16_t off,
		    void *buf, size_t len)
{
	ARG_UNUSED(ctx);
	return logstore_read_snap(s, page, off, buf, len);
}

static const struct logxfer_src log_src = {src_range, src_snapshot, src_read, NULL};
static struct logxfer xfer;
static K_MUTEX_DEFINE(xfer_lock); /* Log Control writes (BT RX) and the sender (work queue) */

static void log_xfer_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(log_xfer_work, log_xfer_fn);

/* Two indication slots on Log Control: a request's response can still await the phone's
 * confirmation when the end indication (or a second response) is due. */
static struct log_ind {
	struct bt_gatt_indicate_params p;
	uint8_t v[LOGX_IND_LEN];
	atomic_t busy;
} log_ind[2];

static void log_ind_done(struct bt_gatt_indicate_params *p)
{
	atomic_clear(&CONTAINER_OF(p, struct log_ind, p)->busy);
}

static int log_indicate(const uint8_t v[LOGX_IND_LEN])
{
	struct bt_conn *c = ble_conn();

	if (!c) {
		return -ENOTCONN;
	}
	for (size_t i = 0; i < ARRAY_SIZE(log_ind); i++) {
		struct log_ind *s = &log_ind[i];

		if (!atomic_cas(&s->busy, 0, 1)) {
			continue;
		}
		memcpy(s->v, v, LOGX_IND_LEN);
		memset(&s->p, 0, sizeof(s->p));
		s->p.attr = &log_svc.attrs[4];
		s->p.data = s->v;
		s->p.len = LOGX_IND_LEN;
		s->p.destroy = log_ind_done;
		int ret = bt_gatt_indicate(c, &s->p);

		if (ret) {
			atomic_clear(&s->busy);
		}
		return ret;
	}
	return -EBUSY;
}

/* Log Data: u32 page sequence, u16 byte offset in the page, then page bytes. The end
 * indication goes on the same ATT bearer after the last of them (no EATT), so it arrives
 * after them. */
static void log_xfer_fn(struct k_work *w)
{
	struct bt_conn *c = ble_conn();
	uint8_t pkt[LOGX_DATA_HDR + LOGX_DATA_MAX];
	uint8_t end[LOGX_IND_LEN];
	k_timeout_t again = K_NO_WAIT;

	ARG_UNUSED(w);
	k_mutex_lock(&xfer_lock, K_FOREVER);
	if (!c) {
		logxfer_cancel(&xfer);
		k_mutex_unlock(&xfer_lock);
		return;
	}
	uint16_t per = (uint16_t)MIN(bt_gatt_get_mtu(c) - 3U - LOGX_DATA_HDR, LOGX_DATA_MAX);

	for (int burst = 0; burst < 8; burst++) {
		size_t n = logxfer_packet(&xfer, &log_src, per, pkt);

		if (n == 0) {
			break;
		}
		if (bt_gatt_notify(c, &log_svc.attrs[7], pkt, (uint16_t)n)) {
			break; /* no buffer: the same packet again after the controller drains */
		}
		logxfer_sent(&xfer, n);
	}
	if (xfer.active) {
		again = K_MSEC(20);
	} else if (logxfer_end(&xfer, end)) {
		ble_fast_link(false);
		if (log_indicate(end) == 0) {
			logxfer_end_sent(&xfer);
		} else {
			again = K_MSEC(50); /* both slots awaiting confirmation */
		}
	}
	k_mutex_unlock(&xfer_lock);
	if (!K_TIMEOUT_EQ(again, K_NO_WAIT)) {
		k_work_reschedule(&log_xfer_work, again);
	}
}

static void erase_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	(void)logstore_erase_all();
}
static K_WORK_DEFINE(erase_work, erase_fn);

/* Factory reset: log, settings and bonds, then reboot. */
static void reset_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	(void)logstore_erase_all();
	(void)settings_delete("atrium/cfg");
	(void)bt_unpair(BT_ID_DEFAULT, NULL);
	sys_reboot(SYS_REBOOT_COLD);
}
static K_WORK_DEFINE(reset_work, reset_fn);

static ssize_t read_log_info(struct bt_conn *c, const struct bt_gatt_attr *a, void *buf,
			     uint16_t len, uint16_t off)
{
	uint32_t o, h;
	uint8_t v[GATT_LOG_INFO_LEN];

	logstore_range(&o, &h);
	gatt_encode_log_info(o, h, logstore_pages(), v);
	return bt_gatt_attr_read(c, a, buf, len, off, v, sizeof(v));
}

static ssize_t write_log_ctrl(struct bt_conn *c, const struct bt_gatt_attr *a, const void *buf,
			      uint16_t len, uint16_t off, uint8_t flags)
{
	uint8_t rsp[LOGX_IND_LEN];

	ARG_UNUSED(c);
	ARG_UNUSED(a);
	ARG_UNUSED(flags);
	if (off || len < 1) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	k_mutex_lock(&xfer_lock, K_FOREVER);
	logxfer_control(&xfer, &log_src, buf, len, rsp);
	bool go = xfer.active;

	k_mutex_unlock(&xfer_lock);
	/* the response first, so the phone has the page count before any Log Data */
	(void)log_indicate(rsp);
	ble_fast_link(go);
	if (go) {
		k_work_reschedule(&log_xfer_work, K_MSEC(5));
	}
	return len;
}

BT_GATT_SERVICE_DEFINE(log_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_log),
	BT_GATT_CHARACTERISTIC(&uuid_log_info.uuid, BT_GATT_CHRC_READ, R_ENC, read_log_info, NULL,
			       NULL),
	BT_GATT_CHARACTERISTIC(&uuid_log_ctrl.uuid, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
			       W_ENC, NULL, write_log_ctrl, NULL),
	BT_GATT_CCC(NULL, CCC_P),
	BT_GATT_CHARACTERISTIC(&uuid_log_data.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL,
			       NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
	BT_GATT_CHARACTERISTIC(&uuid_live_epoch.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL,
			       NULL, NULL),
	BT_GATT_CCC(NULL, CCC_P),
);

/* Live Epoch: each record as it is logged, with its time and the head page. */
void gatt_atrium_live_epoch(uint32_t t, const struct log_record *r)
{
	struct bt_conn *c = ble_conn();
	uint8_t v[GATT_LIVE_EPOCH_LEN];
	uint32_t oldest, head;

	if (!c) {
		return;
	}
	logstore_range(&oldest, &head);
	gatt_encode_live_epoch(t, head, r, v);
	(void)bt_gatt_notify(c, &log_svc.attrs[10], v, sizeof(v));
}

/* ---- 8 Settings ---- */

static ssize_t read_setting(struct bt_conn *c, const struct bt_gatt_attr *a, void *buf,
			    uint16_t len, uint16_t off)
{
	struct app_settings s;
	uint8_t v[GATT_SLEEP_WINDOW_LEN];
	size_t n = 0;

	app_settings_get(&s);
	if (bt_uuid_cmp(a->uuid, &uuid_mode.uuid) == 0) {
		atomic_val_t r = atomic_get(&research);

		v[0] = r ? (uint8_t)r : s.mode;
		n = 1;
	} else if (bt_uuid_cmp(a->uuid, &uuid_time.uuid) == 0) {
		sys_put_le32(app_time_now(), v);
		n = 4;
	} else if (bt_uuid_cmp(a->uuid, &uuid_sleep.uuid) == 0) {
		n = gatt_encode_sleep_window(&s, v);
	} else if (bt_uuid_cmp(a->uuid, &uuid_logint.uuid) == 0) {
		sys_put_le16(LOG_EPOCH_S, v); /* fixed 30 s epoch */
		n = 2;
	}
	return bt_gatt_attr_read(c, a, buf, len, off, v, n);
}

static ssize_t write_setting(struct bt_conn *c, const struct bt_gatt_attr *a, const void *buf,
			     uint16_t len, uint16_t off, uint8_t flags)
{
	const uint8_t *p = buf;
	struct app_settings s;

	ARG_UNUSED(flags);
	if (off) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	app_settings_get(&s);
	if (bt_uuid_cmp(a->uuid, &uuid_mode.uuid) == 0) {
		if (len != 1 || !mode_user_selectable((enum pod_mode)p[0])) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		if (mode_is_research((enum pod_mode)p[0])) {
			struct app_status st;

			/* never on the Plinth, and never armed there for the next lift */
			app_status_get(&st);
			if (st.power.docked) {
				return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
			}
			if (mode_is_vibration((enum pod_mode)p[0])) {
				/* packets sized to this link's MTU from the capture start */
				atomic_set(&vib_inflight, 0);
				sensors_set_vib_sink(vib_sink, bt_gatt_get_mtu(c) - 3U);
			}
			atomic_set(&research, p[0]);
			if (handlers && handlers->settings_changed) {
				handlers->settings_changed();
			}
			return len;
		}
		atomic_set(&research, 0);
		s.mode = p[0];
	} else if (bt_uuid_cmp(a->uuid, &uuid_time.uuid) == 0) {
		if (len != 4) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		app_time_set(sys_get_le32(p));
		return len;
	} else if (bt_uuid_cmp(a->uuid, &uuid_sleep.uuid) == 0) {
		if (gatt_decode_sleep_window(p, len, &s)) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
	}
	(void)prefs_save(&s);
	if (handlers && handlers->settings_changed) {
		handlers->settings_changed();
	}
	return len;
}

static ssize_t write_cmd(struct bt_conn *c, const struct bt_gatt_attr *a, const void *buf,
			 uint16_t len, uint16_t off, uint8_t flags)
{
	const uint8_t *p = buf;

	ARG_UNUSED(a);
	ARG_UNUSED(flags);
	if (off || len != 5 || sys_get_le32(&p[1]) != CMD_KEY) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	switch (p[0]) {
	case 0x01:
		sys_reboot(SYS_REBOOT_COLD);
		break;
	case 0x02:
		if (!handlers || !handlers->ship_now || handlers->ship_now()) {
			return BT_GATT_ERR(ERR_SHIP);
		}
		break;
	case 0x03:
		k_work_submit(&erase_work);
		break;
	case 0x04:
		(void)bt_unpair(BT_ID_DEFAULT, NULL);
		(void)bt_conn_disconnect(c, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		break;
	case 0x05:
		k_work_submit(&reset_work);
		break;
	default:
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	return len;
}

#define RW (BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE)

BT_GATT_SERVICE_DEFINE(set_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_set),
	BT_GATT_CHARACTERISTIC(&uuid_mode.uuid, RW, R_ENC | W_ENC, read_setting, write_setting,
			       NULL),
	BT_GATT_CHARACTERISTIC(&uuid_time.uuid, RW, R_ENC | W_ENC, read_setting, write_setting,
			       NULL),
	BT_GATT_CHARACTERISTIC(&uuid_sleep.uuid, RW, R_ENC | W_ENC, read_setting, write_setting,
			       NULL),
	BT_GATT_CHARACTERISTIC(&uuid_logint.uuid, BT_GATT_CHRC_READ, R_ENC, read_setting, NULL,
			       NULL),
	BT_GATT_CHARACTERISTIC(&uuid_cmd.uuid, BT_GATT_CHRC_WRITE, W_ENC, NULL, write_cmd, NULL),
);

/* ---- periodic notifications ---- */

/* bytes 0-5: states, flags, SoC (the battery estimate is only here, no Battery service) */
static uint8_t last_charge[6];
static uint32_t charge_age_s;

void gatt_atrium_init(const struct ble_handlers *h)
{
	handlers = h;
}

enum pod_mode gatt_atrium_user_mode(uint8_t stored)
{
	atomic_val_t r = atomic_get(&research);

	return r ? (enum pod_mode)r : (enum pod_mode)stored;
}

void gatt_atrium_research_end(void)
{
	atomic_set(&research, 0);
}

void gatt_atrium_disconnected(void)
{
	atomic_set(&research, 0);
	ppg_stream = false;
	motion_stream = false;
	atomic_set(&fit_on, 0);
	sensors_set_ppg_sink(NULL);
	sensors_set_motion_sink(NULL);
	sensors_set_vib_sink(NULL, 0);
	atomic_set(&vib_inflight, 0);
	k_mutex_lock(&xfer_lock, K_FOREVER);
	logxfer_cancel(&xfer);
	k_mutex_unlock(&xfer_lock);
}

void gatt_atrium_tick(const struct app_status *s, bool new_minute, bool temp_due)
{
	struct bt_conn *c = ble_conn();
	uint8_t v[GATT_CHARGE_STATE_LEN];

	if (s->ox_seq != oxygen_seq) {
		oxygen_seq = s->ox_seq;
		gatt_encode_oxygen(s, oxygen);
		if (c) {
			(void)bt_gatt_notify(c, &ppg_svc.attrs[13], oxygen, sizeof(oxygen));
		}
	}
	gatt_encode_fit_quality(s, fitq_val);
	if (!c) {
		return;
	}
	/* 1 Hz while subscribed, off the Plinth; 255 while not measuring */
	if (!s->power.docked) {
		(void)bt_gatt_notify(c, &ppg_svc.attrs[16], fitq_val, sizeof(fitq_val));
	}
	gatt_encode_charge_state(s, v);
	charge_age_s++;
	if (memcmp(v, last_charge, sizeof(last_charge)) ||
	    (s->power.docked && charge_age_s >= 60)) {
		memcpy(last_charge, v, sizeof(last_charge));
		charge_age_s = 0;
		(void)bt_gatt_notify(c, &power_svc.attrs[2], v, sizeof(v));
	}

	pulse_detail[0] = s->bpm;
	pulse_detail[1] = s->bpm_conf;
	sys_put_le16(PULSE_WIN / PULSE_FS, &pulse_detail[2]);
	pulse_detail[4] = 0;
	const struct mode_cfg *m = mode_get(s->power.mode);

	for (uint8_t i = 0; m && i < m->n_slots; i++) {
		if (m->slot[i].led == LED_GREEN) {
			pulse_detail[4] = m->hz;
			break;
		}
	}
	if (s->power.state == POD_WORN) {
		(void)bt_gatt_notify(c, &ppg_svc.attrs[7], pulse_detail, sizeof(pulse_detail));
	}
	if (temp_due) {
		uint8_t t[GATT_SKIN_TEMP_LEN];

		gatt_encode_skin_temp(s, app_time_now(), t);
		(void)bt_gatt_notify(c, &temp_svc.attrs[2], t, sizeof(t));
	}
	if (new_minute) {
		uint8_t a[GATT_ACTIVITY_LEN];

		gatt_encode_activity(s, app_time_now(), a);
		(void)bt_gatt_notify(c, &act_svc.attrs[2], a, sizeof(a));
	}
}
