/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE peripheral: advertising, connection, security, SIG services (DIS, HRS)
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
	/* on the Plinth: ship mode at the next lift; off it: power off until the Plinth */
	int (*ship_now)(void);
};

int ble_init(const struct ble_handlers *h);
int ble_start(void);

/* Accept new pairings for the next `seconds` (pair on lift). Bonded phones reconnect at any
 * time. */
void ble_pairing_window(uint32_t seconds);
void ble_set_mode(enum pod_mode mode);
/* A log read runs: a 15-30 ms interval until it ends, then the mode's again. */
void ble_fast_link(bool on);
struct bt_conn *ble_conn(void);

/* Called once a second from the main loop. */
void ble_tick(const struct app_status *s, bool new_minute, bool temp_due);

/* gatt_atrium.c */
void gatt_atrium_init(const struct ble_handlers *h);
void gatt_atrium_tick(const struct app_status *s, bool new_minute, bool temp_due);
void gatt_atrium_disconnected(void);
/* Fit mode (PPG Control bit 2): on until it is cleared, the phone disconnects, or 180 s. */
bool gatt_atrium_fit(void);
/* The user's mode: a research mode set for this connection, else the stored choice. */
enum pod_mode gatt_atrium_user_mode(uint8_t stored);
/* End a research mode (the pod landed on the Plinth). */
void gatt_atrium_research_end(void);
/* Live Epoch (0x0504): call after a record was logged, with its epoch start. */
struct log_record;
void gatt_atrium_live_epoch(uint32_t t, const struct log_record *r);

#endif /* ATRIUM_BLE_H_ */
