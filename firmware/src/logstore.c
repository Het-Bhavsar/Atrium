/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "logstore.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

#include "prefs.h"

LOG_MODULE_REGISTER(logstore, CONFIG_ATRIUM_LOG_LEVEL);

/* 170 records per 4 KB sector; the last 16 bytes of each sector stay erased */
#define SECTOR    4096U
#define PER_SECT  (SECTOR / LOG_RECORD_SIZE)

static K_MUTEX_DEFINE(lock);
static const struct flash_area *fa;
static uint32_t cap; /* records in the partition */
static uint32_t oldest, next;

static off_t offset_of(uint32_t seq)
{
	uint32_t slot = seq % cap;

	return (off_t)((slot / PER_SECT) * SECTOR + (slot % PER_SECT) * LOG_RECORD_SIZE);
}

static bool erased(const uint8_t *b)
{
	for (int i = 0; i < LOG_RECORD_SIZE; i++) {
		if (b[i] != 0xFF) {
			return false;
		}
	}
	return true;
}

void logstore_restore(uint32_t o, uint32_t n)
{
	oldest = o;
	next = n;
}

int logstore_init(void)
{
	int ret = flash_area_open(PARTITION_ID(log_partition), &fa);

	if (ret) {
		return ret;
	}
	cap = (uint32_t)(fa->fa_size / SECTOR) * PER_SECT;

	/* next is persisted at each sector start: scan forward within the sector */
	k_mutex_lock(&lock, K_FOREVER);
	for (uint32_t i = 0; i < PER_SECT; i++) {
		uint8_t b[LOG_RECORD_SIZE];

		if (next % PER_SECT == 0 && i > 0) {
			break;
		}
		if (flash_area_read(fa, offset_of(next), b, sizeof(b)) || erased(b)) {
			break;
		}
		next++;
	}
	if (next - oldest > cap) {
		oldest = next - cap;
	}
	k_mutex_unlock(&lock);
	LOG_INF("log %u records held, next %u, capacity %u", next - oldest, next, cap);
	return 0;
}

int logstore_append(const struct log_record *r)
{
	uint8_t b[LOG_RECORD_SIZE];
	int ret = 0;

	if (!fa) {
		return -ENODEV;
	}
	log_record_pack(r, b);
	k_mutex_lock(&lock, K_FOREVER);
	if (next % PER_SECT == 0) {
		ret = flash_area_erase(fa, offset_of(next), SECTOR);
		if (next + PER_SECT - oldest > cap) {
			oldest = next + PER_SECT - cap; /* the sector just erased held these */
		}
		prefs_log_seq(oldest, next);
	}
	ret = ret ? ret : flash_area_write(fa, offset_of(next), b, sizeof(b));
	if (ret == 0) {
		next++;
	}
	k_mutex_unlock(&lock);
	return ret;
}

int logstore_read(uint32_t seq, uint8_t out[LOG_RECORD_SIZE])
{
	int ret;

	k_mutex_lock(&lock, K_FOREVER);
	ret = (seq < oldest || seq >= next || !fa) ? -ERANGE
						   : flash_area_read(fa, offset_of(seq), out,
								     LOG_RECORD_SIZE);
	k_mutex_unlock(&lock);
	return ret;
}

uint32_t logstore_capacity(void)
{
	return cap;
}

void logstore_range(uint32_t *o, uint32_t *n)
{
	k_mutex_lock(&lock, K_FOREVER);
	*o = oldest;
	*n = next;
	k_mutex_unlock(&lock);
}

void logstore_release(uint32_t upto)
{
	k_mutex_lock(&lock, K_FOREVER);
	if (upto > oldest) {
		oldest = MIN(upto, next);
		prefs_log_seq(oldest, next);
	}
	k_mutex_unlock(&lock);
}

int logstore_erase_all(void)
{
	int ret;

	k_mutex_lock(&lock, K_FOREVER);
	ret = fa ? flash_area_erase(fa, 0, fa->fa_size) : -ENODEV;
	/* sequence numbers keep growing so the app never sees one reused */
	next = ROUND_UP(next, PER_SECT);
	oldest = next;
	prefs_log_seq(oldest, next);
	k_mutex_unlock(&lock);
	return ret;
}
