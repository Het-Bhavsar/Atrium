/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "logstore.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

#include "prefs.h"

LOG_MODULE_REGISTER(logstore, CONFIG_ATRIUM_LOG_LEVEL);

static K_MUTEX_DEFINE(lock);
static const struct flash_area *fa;
static struct logpack lp;
static struct logpack_state saved;
static bool have_saved;
static bool ready;

void logstore_restore(const struct logpack_state *st)
{
	saved = *st;
	have_saved = true;
}

static int fl_read(void *ctx, uint32_t off, void *buf, size_t len)
{
	return flash_area_read(ctx, off, buf, len);
}

static int fl_write(void *ctx, uint32_t off, const void *buf, size_t len)
{
	return flash_area_write(ctx, off, buf, len);
}

static int fl_erase(void *ctx, uint32_t off, size_t len)
{
	return flash_area_erase(ctx, off, len);
}

static void fl_persist(void *ctx, const struct logpack_state *st)
{
	ARG_UNUSED(ctx);
	prefs_log_state(st);
}

static struct logpack_flash fl = {
	.read = fl_read,
	.write = fl_write,
	.erase = fl_erase,
	.persist = fl_persist,
};

int logstore_init(void)
{
	int ret = flash_area_open(PARTITION_ID(log_partition), &fa);

	if (ret) {
		return ret;
	}
	fl.ctx = (void *)fa;
	k_mutex_lock(&lock, K_FOREVER);
	ret = logpack_mount(&lp, &fl, fa->fa_size / LOG_PAGE_SIZE, have_saved ? &saved : NULL);
	ready = ret == 0;
	k_mutex_unlock(&lock);
	LOG_INF("log pages %u..%u of %u, offset %u (%d)", lp.st.oldest, lp.st.head, lp.pages,
		lp.off, ret);
	return ret;
}

int logstore_append(uint32_t t, const struct log_record *r)
{
	uint8_t b[LOG_RECORD_SIZE];
	int ret;

	if (!ready) {
		return -ENODEV;
	}
	log_record_pack(r, b);
	k_mutex_lock(&lock, K_FOREVER);
	ret = logpack_append(&lp, t, b);
	k_mutex_unlock(&lock);
	return ret;
}

int logstore_close(void)
{
	int ret;

	if (!ready) {
		return -ENODEV;
	}
	k_mutex_lock(&lock, K_FOREVER);
	ret = logpack_close(&lp);
	k_mutex_unlock(&lock);
	return ret;
}

int logstore_erase_all(void)
{
	int ret;

	if (!ready) {
		return -ENODEV;
	}
	k_mutex_lock(&lock, K_FOREVER);
	ret = logpack_erase_all(&lp);
	k_mutex_unlock(&lock);
	return ret;
}

void logstore_range(uint32_t *oldest, uint32_t *head)
{
	k_mutex_lock(&lock, K_FOREVER);
	*oldest = lp.st.oldest;
	*head = lp.st.head;
	k_mutex_unlock(&lock);
}

uint32_t logstore_pages(void)
{
	return lp.pages;
}

int logstore_read(uint32_t page, uint16_t off, void *buf, size_t len)
{
	int ret;

	if (!ready) {
		return -ENODEV;
	}
	k_mutex_lock(&lock, K_FOREVER);
	ret = logpack_read_page(&lp, page, off, buf, len);
	k_mutex_unlock(&lock);
	return ret;
}

void logstore_snapshot(struct logpack_snap *s)
{
	k_mutex_lock(&lock, K_FOREVER);
	logpack_snapshot(&lp, s);
	k_mutex_unlock(&lock);
}

int logstore_read_snap(const struct logpack_snap *s, uint32_t page, uint16_t off, void *buf,
		       size_t len)
{
	int ret;

	if (!ready) {
		return -ENODEV;
	}
	k_mutex_lock(&lock, K_FOREVER);
	ret = logpack_read_snap(&lp, s, page, off, buf, len);
	k_mutex_unlock(&lock);
	return ret;
}
