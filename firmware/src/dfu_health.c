/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "dfu_health.h"

enum dfu_action dfu_health_decide(bool confirmed, uint32_t uptime_s, bool healthy)
{
	if (confirmed) {
		return DFU_WAIT;
	}
	if (healthy && uptime_s >= DFU_CONFIRM_S) {
		return DFU_CONFIRM;
	}
	if (!healthy && uptime_s >= DFU_REVERT_S) {
		return DFU_REVERT;
	}
	return DFU_WAIT;
}
