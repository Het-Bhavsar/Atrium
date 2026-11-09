/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Byte layouts of docs/ble_gatt.md. Plain C, so the host tests pin the contract.
 */
#ifndef ATRIUM_GATT_ENCODE_H_
#define ATRIUM_GATT_ENCODE_H_

#include <stddef.h>
#include <stdint.h>

#include "app_state.h"

struct log_record;

#define GATT_CHARGE_STATE_LEN 19
#define GATT_SKIN_TEMP_LEN    9
#define GATT_ACTIVITY_LEN     10
#define GATT_OXYGEN_LEN       8
#define GATT_LOG_INFO_LEN     18
#define GATT_SLEEP_WINDOW_LEN 6
#define GATT_FIT_QUALITY_LEN  4
#define GATT_LIVE_EPOCH_LEN   20 /* fits a 23-byte ATT MTU */

/* 3.1 */
size_t gatt_encode_charge_state(const struct app_status *s, uint8_t out[GATT_CHARGE_STATE_LEN]);
/* 5 */
size_t gatt_encode_skin_temp(const struct app_status *s, uint32_t time,
			     uint8_t out[GATT_SKIN_TEMP_LEN]);
/* 6 */
size_t gatt_encode_activity(const struct app_status *s, uint32_t time,
			    uint8_t out[GATT_ACTIVITY_LEN]);
/* 4.2 Oxygen Ratio: relative red/IR ratio, research only */
size_t gatt_encode_oxygen(const struct app_status *s, uint8_t out[GATT_OXYGEN_LEN]);
/* 4.2 Fit Quality: the fit guide's live bar (FEATURES.md #24) */
size_t gatt_encode_fit_quality(const struct app_status *s, uint8_t out[GATT_FIT_QUALITY_LEN]);
/* 4.2 Motion Data rate field: mHz up to 32767; above it bit 15 set and bits 0-14 in 10 mHz
 * (104 Hz = 0xA8A0), saturating at 327.67 Hz */
uint16_t gatt_motion_rate(uint32_t rate_mhz);
/* 7 */
size_t gatt_encode_log_info(uint32_t oldest, uint32_t head, uint32_t pages,
			    uint8_t out[GATT_LOG_INFO_LEN]);
/* 7 Live Epoch: u32 epoch start (the log's time, bit 31 = uptime), u32 head page, then the
 * 12 record bytes exactly as logged */
size_t gatt_encode_live_epoch(uint32_t t, uint32_t head, const struct log_record *r,
			      uint8_t out[GATT_LIVE_EPOCH_LEN]);
/* 8: sleep window both ways; decode returns -1 on a bad value */
size_t gatt_encode_sleep_window(const struct app_settings *s, uint8_t out[GATT_SLEEP_WINDOW_LEN]);
int gatt_decode_sleep_window(const uint8_t *in, size_t len, struct app_settings *s);

#endif /* ATRIUM_GATT_ENCODE_H_ */
