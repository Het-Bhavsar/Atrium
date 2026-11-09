/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * ble_policy.h: which links the pod keeps, and the pairing window (docs/ble_gatt.md).
 */
#include <zephyr/ztest.h>

#include "ble_policy.h"

ZTEST_SUITE(ble_policy, NULL, NULL, NULL, NULL, NULL);

ZTEST(ble_policy, test_link_secure)
{
	zassert_true(ble_link_secure(2, true, 16), "LE Secure Connections, Just Works");
	zassert_true(ble_link_secure(4, true, 16), "LESC with a passkey");
	zassert_false(ble_link_secure(2, false, 16), "a legacy bond");
	zassert_false(ble_link_secure(3, false, 16), "legacy passkey");
	zassert_false(ble_link_secure(2, true, 7), "a short key");
	zassert_false(ble_link_secure(1, true, 16), "not encrypted");
}

/* One window for a new phone and for a phone pairing again over its lost bond */
ZTEST(ble_policy, test_pairing_window)
{
	zassert_true(ble_pairing_open(60000, 60000), "the last moment of the window");
	zassert_false(ble_pairing_open(60001, 60000), "after it");
	zassert_false(ble_pairing_open(0, -1), "no window yet");
}
