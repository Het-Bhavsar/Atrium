/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Link security rules for ble.c (docs/ble_gatt.md, Conventions: Security). Plain C, no
 * Zephyr, so the host tests cover them.
 *  - A link is accepted only when it is encrypted (level 2 or more, Zephyr's BT_SECURITY_L2)
 *    with an LE Secure Connections key of 16 bytes. Legacy pairing is off in the build
 *    (CONFIG_BT_SMP_SC_PAIR_ONLY); this also refuses a legacy bond made by an older build.
 *  - A link that is still not encrypted BLE_UNENC_TIMEOUT_S after it connects is dropped, so a
 *    stranger cannot hold the pod's one connection.
 *  - New pairings only up to the end of the window after a lift from the Plinth (or boot). That
 *    includes a phone pairing again over its own old bond (CONFIG_BT_SMP_ALLOW_UNAUTH_OVERWRITE).
 */
#ifndef ATRIUM_BLE_POLICY_H_
#define ATRIUM_BLE_POLICY_H_

#include <stdbool.h>
#include <stdint.h>

#define BLE_UNENC_TIMEOUT_S 30
#define BLE_MIN_KEY_SIZE    16

static inline bool ble_link_secure(uint8_t level, bool sc, uint8_t key_size)
{
	return level >= 2 && sc && key_size >= BLE_MIN_KEY_SIZE;
}

static inline bool ble_pairing_open(int64_t now_ms, int64_t until_ms)
{
	return now_ms <= until_ms;
}

#endif /* ATRIUM_BLE_POLICY_H_ */
