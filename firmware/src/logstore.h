/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ring of 24-byte records in the MX25R1635F "log" partition. Records are
 * addressed by a sequence number that only grows; the oldest 4 KB sector is
 * erased when the ring wraps.
 */
#ifndef ATRIUM_LOGSTORE_H_
#define ATRIUM_LOGSTORE_H_

#include <stdint.h>

#include "log_record.h"

int logstore_init(void);
int logstore_append(const struct log_record *r);
/* Read one record by sequence number: -ERANGE if it is not held. */
int logstore_read(uint32_t seq, uint8_t out[LOG_RECORD_SIZE]);
void logstore_range(uint32_t *oldest, uint32_t *next);
/* Records the partition holds. */
uint32_t logstore_capacity(void);
/* Records below `upto` are downloaded and may be dropped. */
void logstore_release(uint32_t upto);
int logstore_erase_all(void);

/* Sequence numbers to persist (prefs.c). */
void logstore_restore(uint32_t oldest, uint32_t next);

#endif /* ATRIUM_LOGSTORE_H_ */
