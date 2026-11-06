/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ble.h"

#include <stdio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(ble, CONFIG_ATRIUM_LOG_LEVEL);

#define POWER_SVC_UUID BT_UUID_128_ENCODE(0x8f2a0100, 0x4c46, 0x4154, 0x5249, 0x554d2e706f64)

static struct bt_conn *conn;
static enum pod_mode cur_mode = MODE_SMART;
static uint8_t last_soc = 0xFF;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, POWER_SVC_UUID),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static uint16_t adv_units(enum pod_mode m)
{
	/* 2 s docked (counted in the docked-idle budget), 1 s otherwise [ASSUMED] */
	return m == MODE_DOCKED_IDLE ? BT_GAP_MS_TO_ADV_INTERVAL(2000)
				     : BT_GAP_MS_TO_ADV_INTERVAL(1000);
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

	(void)bt_conn_le_param_update(conn, &p);
}

static void connected(struct bt_conn *c, uint8_t err)
{
	if (err) {
		LOG_WRN("connect failed 0x%02x", err);
		return;
	}
	conn = bt_conn_ref(c);
	/* encryption before any Atrium characteristic works */
	(void)bt_conn_set_security(c, BT_SECURITY_L2);
	conn_params_for_mode();
}

static void disconnected(struct bt_conn *c, uint8_t reason)
{
	ARG_UNUSED(reason);
	if (conn == c) {
		bt_conn_unref(conn);
		conn = NULL;
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

int ble_init(const struct ble_handlers *h)
{
	int ret = bt_enable(NULL);

	if (ret) {
		return ret;
	}
	(void)bt_conn_auth_info_cb_register(&auth_info);
	gatt_atrium_init(h);

	/* DIS serial number: FICR DEVICEID as 16 hex digits */
	uint8_t id[8];
	char serial[17];
	ssize_t n = hwinfo_get_device_id(id, sizeof(id));

	if (n == sizeof(id)) {
		for (int i = 0; i < 8; i++) {
			snprintf(&serial[2 * i], 3, "%02X", id[i]);
		}
		(void)settings_runtime_set("bt/dis/serial", serial, 16);
	}
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
	} else {
		(void)adv_start();
	}
}

struct bt_conn *ble_conn(void)
{
	return conn;
}

void ble_tick(const struct app_status *s, bool new_minute, bool temp_due)
{
	if (s->soc != last_soc) {
		last_soc = s->soc;
		(void)bt_bas_set_battery_level(MIN(s->soc, 100));
	}
	if (conn && s->power.state == POD_WORN && s->bpm) {
		(void)bt_hrs_notify(s->bpm);
	}
	gatt_atrium_tick(s, new_minute, temp_due);
}
