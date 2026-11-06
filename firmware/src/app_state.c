/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_state.h"

#include <zephyr/kernel.h>

static K_MUTEX_DEFINE(lock);
static struct app_status status;
static struct app_settings settings = APP_SETTINGS_DEFAULT;
static int64_t time_offset_ms; /* unix ms - uptime ms */
static bool time_set;

void app_status_get(struct app_status *out)
{
	k_mutex_lock(&lock, K_FOREVER);
	*out = status;
	k_mutex_unlock(&lock);
}

void app_status_update(void (*fn)(struct app_status *s, void *arg), void *arg)
{
	k_mutex_lock(&lock, K_FOREVER);
	fn(&status, arg);
	k_mutex_unlock(&lock);
}

void app_settings_get(struct app_settings *out)
{
	k_mutex_lock(&lock, K_FOREVER);
	*out = settings;
	k_mutex_unlock(&lock);
}

void app_settings_set(const struct app_settings *in)
{
	k_mutex_lock(&lock, K_FOREVER);
	settings = *in;
	k_mutex_unlock(&lock);
}

uint32_t app_time_now(void)
{
	int64_t up = k_uptime_get();

	if (!time_set) {
		return (uint32_t)(up / 1000) | 0x80000000U;
	}
	return (uint32_t)((up + time_offset_ms) / 1000);
}

void app_time_set(uint32_t unix_s)
{
	time_offset_ms = (int64_t)unix_s * 1000 - k_uptime_get();
	time_set = true;
}

bool app_time_is_set(void)
{
	return time_set;
}

bool app_is_night(const struct app_settings *s)
{
	if (!time_set || s->sleep_start == s->sleep_end) {
		return false;
	}
	int32_t local = (int32_t)(app_time_now() / 60U % 1440U) + s->utc_offset_min;
	uint16_t m = (uint16_t)((local % 1440 + 1440) % 1440);

	if (s->sleep_start < s->sleep_end) {
		return m >= s->sleep_start && m < s->sleep_end;
	}
	return m >= s->sleep_start || m < s->sleep_end;
}
