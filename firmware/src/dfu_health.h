/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * After a BLE update, MCUboot boots the new image once, unconfirmed. On Rev B.2,
 * BLE is the only field path, so a bad image must never stick:
 *  - confirm only once the image has run healthily for DFU_CONFIRM_S;
 *  - if it is still unhealthy at DFU_REVERT_S, reboot, and MCUboot swaps
 *    the previous image back.
 * Healthy means BLE up, PPG and IMU ready, and at least one gauge reading.
 * Plain C, no Zephyr.
 */
#ifndef ATRIUM_DFU_HEALTH_H_
#define ATRIUM_DFU_HEALTH_H_

#include <stdbool.h>
#include <stdint.h>

#define DFU_CONFIRM_S 60
#define DFU_REVERT_S  300

enum dfu_action {
	DFU_WAIT = 0,
	DFU_CONFIRM,
	DFU_REVERT,
};

enum dfu_action dfu_health_decide(bool confirmed, uint32_t uptime_s, bool healthy);

#endif /* ATRIUM_DFU_HEALTH_H_ */
