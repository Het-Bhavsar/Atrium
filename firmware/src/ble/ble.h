/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE peripheral: advertising, connection, security, SIG services (DIS, BAS, HRS)
 * and the Atrium services of docs/ble_gatt.md.
 */
#ifndef ATRIUM_BLE_H_
#define ATRIUM_BLE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/conn.h>

#include "app_state.h"
#include "modes.h"

struct ble_handlers {
	void (*settings_changed)(void);
	/* returns 0, or -EBUSY on the dock */
	int (*ship_now)(void);
};

int ble_init(const struct ble_handlers *h);
int ble_start(void);
void ble_set_mode(enum pod_mode mode);
struct bt_conn *ble_conn(void);

/* Called once a second from the main loop. */
void ble_tick(const struct app_status *s, bool new_minute, bool temp_due);

/* gatt_atrium.c */
void gatt_atrium_init(const struct ble_handlers *h);
void gatt_atrium_tick(const struct app_status *s, bool new_minute, bool temp_due);
void gatt_atrium_disconnected(void);

#endif /* ATRIUM_BLE_H_ */
