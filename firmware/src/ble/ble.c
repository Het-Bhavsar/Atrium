/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ble.h"
#include "ble_policy.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(ble, CONFIG_ATRIUM_LOG_LEVEL);

#define POWER_SVC_UUID BT_UUID_128_ENCODE(0x8f2a0100, 0x4c46, 0x4154, 0x5249, 0x554d2e706f64)

static struct bt_conn *conn;
static int64_t pair_until_ms;
static enum pod_mode cur_mode = MODE_SMART;
/* a log read runs: the shortest interval phones accept, back to the mode's when it ends */
static bool fast_link;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, POWER_SVC_UUID),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static uint16_t adv_units(enum pod_mode m)
{
	/* 2 s docked or off the body, 1 s otherwise (ble_policy.h) */
	return BT_GAP_MS_TO_ADV_INTERVAL(ble_adv_interval_ms(m));
}

static int adv_start(void)
{
	struct bt_le_adv_param p = BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONN, adv_units(cur_mode),
						       adv_units(cur_mode) + 16, NULL);

	(void)bt_le_adv_stop();
	return bt_le_adv_start(&p, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
}

static void conn_params_for_mode(void)
{
	const struct mode_cfg *m = mode_get(cur_mode);

	if (!conn || !m) {
		return;
	}
	uint16_t ci = BT_GAP_MS_TO_CONN_INTERVAL(MIN(m->ci_ms, 4000));
	/* supervision timeout must exceed (1 + latency) * 2 * CI: 6 x CI, 10 ms units */
	uint16_t timeout = (uint16_t)MIN(3200, MAX(400, m->ci_ms * 6 / 10));
	struct bt_le_conn_param p = BT_LE_CONN_PARAM_INIT(ci, ci, 0, timeout);
	/* 15 to 30 ms, no latency, 4 s timeout: inside Apple's accessory rules [VERIFY on iOS] */
	struct bt_le_conn_param f = BT_LE_CONN_PARAM_INIT(12, 24, 0, 400);

	(void)bt_conn_le_param_update(conn, fast_link ? &f : &p);
}

/* A link not encrypted in time is dropped, so a stranger cannot hold the one connection */
static void unenc_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	if (conn && bt_conn_get_security(conn) < BT_SECURITY_L2) {
		LOG_WRN("not encrypted after %d s", BLE_UNENC_TIMEOUT_S);
		(void)bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
	}
}
static K_WORK_DELAYABLE_DEFINE(unenc_work, unenc_fn);

static void connected(struct bt_conn *c, uint8_t err)
{
	if (err) {
		LOG_WRN("connect failed 0x%02x", err);
		return;
	}
	conn = bt_conn_ref(c);
	/* encryption before any Atrium characteristic works. L2 with legacy pairing off is LE
	 * Secure Connections; L3 and L4 need a passkey the pod cannot show. */
	(void)bt_conn_set_security(c, BT_SECURITY_L2);
	k_work_reschedule(&unenc_work, K_SECONDS(BLE_UNENC_TIMEOUT_S));
	conn_params_for_mode();
}

static void security_changed(struct bt_conn *c, bt_security_t level, enum bt_security_err err)
{
	struct bt_conn_info info;

	if (err || bt_conn_get_info(c, &info)) {
		return; /* not encrypted: the timeout drops it */
	}
	if (!ble_link_secure((uint8_t)level, info.security.flags & BT_SECURITY_FLAG_SC,
			     info.security.enc_key_size)) {
		LOG_WRN("weak link: level %d, flags 0x%x, key %u", level, info.security.flags,
			info.security.enc_key_size);
		(void)bt_conn_disconnect(c, BT_HCI_ERR_AUTH_FAIL);
		return;
	}
	(void)k_work_cancel_delayable(&unenc_work);
}

static void disconnected(struct bt_conn *c, uint8_t reason)
{
	ARG_UNUSED(reason);
	(void)k_work_cancel_delayable(&unenc_work);
	if (conn == c) {
		bt_conn_unref(conn);
		conn = NULL;
		fast_link = false;
	}
	gatt_atrium_disconnected();
}

static void recycled(void)
{
	(void)adv_start();
}

BT_CONN_CB_DEFINE(conn_cb) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
	.recycled = recycled,
};

static void pairing_complete(struct bt_conn *c, bool bonded)
{
	ARG_UNUSED(c);
	LOG_INF("paired, bonded %d", bonded);
}

static struct bt_conn_auth_info_cb auth_info = {
	.pairing_complete = pairing_complete,
};

/* Replacing a bond without the window would let anyone with a phone's address take its place */
BUILD_ASSERT(!IS_ENABLED(CONFIG_BT_SMP_ALLOW_UNAUTH_OVERWRITE) ||
		     IS_ENABLED(CONFIG_BT_SMP_APP_PAIRING_ACCEPT),
	     "bond overwrite needs the pairing window");

/* New pairings, and a new bond in place of an old one, only inside the window after a lift
 * from the Plinth (no button on the pod). Zephyr checks the overwrite before this call. */
static enum bt_security_err pairing_accept(struct bt_conn *c,
					   const struct bt_conn_pairing_feat *const feat)
{
	ARG_UNUSED(c);
	ARG_UNUSED(feat);
	if (!ble_pairing_open(k_uptime_get(), pair_until_ms)) {
		LOG_WRN("pairing outside the window");
		return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
	}
	return BT_SECURITY_ERR_SUCCESS;
}

static struct bt_conn_auth_cb auth_cb = {
	.pairing_accept = pairing_accept,
};

void ble_pairing_window(uint32_t seconds)
{
	pair_until_ms = k_uptime_get() + (int64_t)seconds * 1000;
	LOG_INF("pairing open %u s", seconds);
}

int ble_init(const struct ble_handlers *h)
{
	int ret = bt_enable(NULL);

	if (ret) {
		return ret;
	}
	(void)bt_conn_auth_cb_register(&auth_cb);
	(void)bt_conn_auth_info_cb_register(&auth_info);
	gatt_atrium_init(h);

	/* no serial number in Device Information: it is open without encryption, and a fixed
	 * identifier would undo the private address */
	/* version + commit: recordings carry the build, so data taken before the IMU rotation is
	 * final can be rotated afterwards */
	(void)settings_runtime_set("bt/dis/fw", ATRIUM_FW_REV, sizeof(ATRIUM_FW_REV) - 1);
	return 0;
}

int ble_start(void)
{
	return adv_start();
}

void ble_set_mode(enum pod_mode mode)
{
	if (mode == cur_mode) {
		return;
	}
	cur_mode = mode;
	if (conn) {
		conn_params_for_mode();
		/* Vibration Data needs 251-byte link packets (DLE); 2M halves the air time. This
		 * build does not ask for either on connect (no BT_AUTO_*_UPDATE), and a phone may
		 * not, so ask here; a central that cannot keeps what it has */
		if (mode_is_vibration(mode)) {
			(void)bt_conn_le_data_len_update(conn, BT_LE_DATA_LEN_PARAM_MAX);
			(void)bt_conn_le_phy_update(conn, BT_CONN_LE_PHY_PARAM_2M);
		}
	} else {
		(void)adv_start();
	}
}

void ble_fast_link(bool on)
{
	if (on == fast_link) {
		return;
	}
	fast_link = on;
	conn_params_for_mode();
}

struct bt_conn *ble_conn(void)
{
	return conn;
}

void ble_tick(const struct app_status *s, bool new_minute, bool temp_due)
{
	if (conn && s->live_bpm) {
		(void)bt_hrs_notify(s->live_bpm);
	}
	gatt_atrium_tick(s, new_minute, temp_due);
}
