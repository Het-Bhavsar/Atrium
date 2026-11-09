/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The log in the internal-flash "log" partition (8 pages, 32 KB): logpack.h on Zephyr's
 * flash map, with a lock for the BLE download and the page state kept in settings.
 */
#ifndef ATRIUM_LOGSTORE_H_
#define ATRIUM_LOGSTORE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "logpack.h"

/* Call after prefs_init(), which hands over the saved page state. */
int logstore_init(void);
int logstore_append(uint32_t t, const struct log_record *r);
int logstore_close(void);
int logstore_erase_all(void);

/* Pages held: oldest .. head (the head is being written). */
void logstore_range(uint32_t *oldest, uint32_t *head);
uint32_t logstore_pages(void);
int logstore_read(uint32_t page, uint16_t off, void *buf, size_t len);
/* The download's view of the head page (logpack.h, struct logpack_snap). */
void logstore_snapshot(struct logpack_snap *s);
int logstore_read_snap(const struct logpack_snap *s, uint32_t page, uint16_t off, void *buf,
		       size_t len);

/* prefs.c: the saved state, before logstore_init(). */
void logstore_restore(const struct logpack_state *st);

#endif /* ATRIUM_LOGSTORE_H_ */
