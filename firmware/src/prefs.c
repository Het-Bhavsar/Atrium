/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "prefs.h"

#include <errno.h>
#include <string.h>
#include <zephyr/settings/settings.h>

#include "logstore.h"

static struct app_settings loaded = APP_SETTINGS_DEFAULT;

static int set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	const char *next;

	if (settings_name_steq(name, "cfg", &next) && !next) {
		struct app_settings s;

		if (len == sizeof(s) && read_cb(cb_arg, &s, sizeof(s)) == sizeof(s)) {
			loaded = s;
		}
		return 0;
	}
	if (settings_name_steq(name, "logpg", &next) && !next) {
		struct logpack_state st;

		if (len == sizeof(st) && read_cb(cb_arg, &st, sizeof(st)) == sizeof(st)) {
			logstore_restore(&st);
		}
		return 0;
	}
	return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(atrium, "atrium", NULL, set, NULL, NULL);

int prefs_init(void)
{
	int ret = settings_subsys_init();

	ret = ret ? ret : settings_load();
	app_settings_set(&loaded);
	return ret;
}

int prefs_save(const struct app_settings *s)
{
	app_settings_set(s);
	return settings_save_one("atrium/cfg", s, sizeof(*s));
}

void prefs_log_state(const struct logpack_state *st)
{
	(void)settings_save_one("atrium/logpg", st, sizeof(*st));
}
