/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * User settings and the log page state in internal flash (Zephyr settings on NVS,
 * "storage" partition, 8 KB). Bonds live there too (bt/...).
 */
#ifndef ATRIUM_PREFS_H_
#define ATRIUM_PREFS_H_

#include <stdint.h>

#include "app_state.h"
#include "logpack.h"

/* Loads everything, including BT bonds. Call after bt_enable(). */
int prefs_init(void);
int prefs_save(const struct app_settings *s);
void prefs_log_state(const struct logpack_state *st);

#endif /* ATRIUM_PREFS_H_ */
