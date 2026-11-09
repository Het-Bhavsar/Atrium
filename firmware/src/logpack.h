/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * The log as a ring of 4 KB flash pages holding blocks of 12-byte records
 * (app/python/atrium_ml/logrec.py, POD_CONTRACT section 4b):
 *  - block: u32 time of the first epoch, u16 block sequence, u8 format (1), u8 record count,
 *    then up to 255 records, back to back. Epoch k of a block starts at time + 30 k;
 *  - a block never crosses a page. When fewer than 20 bytes are left, the page tail stays
 *    erased and the next block opens on the next page. A gap-free page holds 340 records;
 *  - a new block opens at any gap in time (dock, sampling stopped, reboot, clock set), at
 *    255 records and at a page boundary;
 *  - flash cannot be rewritten without an erase, so the second header word stays erased
 *    while the block is open and is written when it closes. A block's first record is
 *    written before its time, so a block with a time always has a record;
 *  - each record is written last word first (byte 11 marks it written), so a record cut
 *    by a reset counts as written; on boot its unwritten words are set to 0 and the
 *    block is closed.
 * Pages carry a sequence number that only grows; page n sits at n % pages. The head page
 * (the one being written), the oldest page held and the next block sequence are persisted
 * by the caller whenever a page opens (logpack_flash.persist).
 * Plain C over a flash interface, so the host tests cover power loss at any write.
 */
#ifndef ATRIUM_LOGPACK_H_
#define ATRIUM_LOGPACK_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "log_record.h"

#ifdef __cplusplus
extern "C" {
#endif

struct logpack_state {
	uint32_t head;   /* page sequence being written */
	uint32_t oldest; /* oldest page sequence held */
	uint16_t seq;    /* next block sequence */
};

struct logpack_flash {
	int (*read)(void *ctx, uint32_t off, void *buf, size_t len);
	int (*write)(void *ctx, uint32_t off, const void *buf, size_t len);
	int (*erase)(void *ctx, uint32_t off, size_t len);
	void (*persist)(void *ctx, const struct logpack_state *st);
	void *ctx;
};

struct logpack {
	const struct logpack_flash *fl;
	uint32_t pages;
	struct logpack_state st;
	uint16_t off;      /* write offset in the head page */
	bool open;
	uint16_t blk_off;  /* header of the open block */
	uint32_t blk_t0;
	uint8_t blk_n;
	uint16_t blk_seq;
};

/*
 * Start on a partition of `pages` pages. With saved state (from persist), the head page is
 * scanned: an open block is closed and writing resumes after it. Without (first boot, or
 * settings lost), the whole log is erased and starts at page 0.
 */
int logpack_mount(struct logpack *lp, const struct logpack_flash *fl, uint32_t pages,
		  const struct logpack_state *saved);

/* Append the record of the epoch that starts at time t (s). */
int logpack_append(struct logpack *lp, uint32_t t, const uint8_t rec[LOG_RECORD_SIZE]);

/* Close the open block, if any (a gap is coming: dock, power off). */
int logpack_close(struct logpack *lp);

/* Erase everything. Page sequence numbers keep growing, so the app never sees one reused. */
int logpack_erase_all(struct logpack *lp);

/* Read part of a held page, by page sequence: -ERANGE when it is not held. */
int logpack_read_page(struct logpack *lp, uint32_t page, uint16_t off, void *buf, size_t len);

/*
 * The head page as it stood at one moment, for a download that reads it in many pieces while
 * records keep arriving. Without it a piece read before an append and the next piece read
 * after it can join half of an erased record to the written half: byte 11 then marks a
 * record whose first bytes read 0xFF. Records before `off` are whole (an append runs under
 * the caller's lock); bytes from `off` on, and the open block's second header word, read as
 * erased, as they were then.
 */
struct logpack_snap {
	uint32_t head;   /* the head page then */
	uint16_t off;    /* its bytes written then */
	uint16_t w1_off; /* the open block's second header word; LOG_PAGE_SIZE = no open block */
};

void logpack_snapshot(const struct logpack *lp, struct logpack_snap *s);

/* logpack_read_page() with the head page as at the snapshot (other pages are final). */
int logpack_read_snap(struct logpack *lp, const struct logpack_snap *s, uint32_t page,
		      uint16_t off, void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_LOGPACK_H_ */
