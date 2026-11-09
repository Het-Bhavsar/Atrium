/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "logpack.h"

#include <errno.h>
#include <string.h>

#define MIN_BLOCK (LOG_HEADER_SIZE + LOG_RECORD_SIZE)
#define ERASED32  0xFFFFFFFFUL

static uint32_t base(const struct logpack *lp, uint32_t page)
{
	return (page % lp->pages) * LOG_PAGE_SIZE;
}

static int rd(struct logpack *lp, uint32_t off, void *buf, size_t len)
{
	return lp->fl->read(lp->fl->ctx, off, buf, len);
}

static int wr(struct logpack *lp, uint32_t off, const void *buf, size_t len)
{
	return lp->fl->write(lp->fl->ctx, off, buf, len);
}

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static bool erased(struct logpack *lp, uint32_t off, size_t len)
{
	uint8_t b[16];

	while (len) {
		size_t n = len < sizeof(b) ? len : sizeof(b);

		if (rd(lp, off, b, n)) {
			return false;
		}
		for (size_t i = 0; i < n; i++) {
			if (b[i] != 0xFF) {
				return false;
			}
		}
		off += n;
		len -= n;
	}
	return true;
}

/* A record is written last word first: bytes 8-11 carry the "written" marker, so a record
 * cut short by a reset is still counted and never read as a block header. Bytes 0-3 and
 * 4-7 are never all 0xFF in a whole record (RMSSD stops at 254, byte 5 at 0xF7). */
static int put_rec(struct logpack *lp, uint32_t at, const uint8_t rec[LOG_RECORD_SIZE])
{
	int ret = wr(lp, at + 8, &rec[8], 4);

	return ret ? ret : wr(lp, at, rec, 8);
}

/* After a reset: zero the words of a cut record that never got written (0 = "none" in
 * every field there). They are still erased, so this is their first write. */
static int mend_rec(struct logpack *lp, uint32_t at)
{
	static const uint8_t zero[4];
	uint8_t w[4];
	int ret = 0;

	for (uint32_t k = 0; k < 8 && !ret; k += 4) {
		ret = rd(lp, at + k, w, sizeof(w));
		if (!ret && get32(w) == ERASED32) {
			ret = wr(lp, at + k, zero, sizeof(zero));
		}
	}
	return ret;
}

static int write_w1(struct logpack *lp, uint32_t at, uint16_t seq, uint8_t n)
{
	uint8_t w[4] = {(uint8_t)seq, (uint8_t)(seq >> 8), LOG_FORMAT, n};

	return wr(lp, at + 4, w, sizeof(w));
}

/* Next page: erase it, then persist, then write (a reset in between leaves either the old
 * head intact or an erased new head). */
static int next_page(struct logpack *lp)
{
	uint32_t page = lp->st.head + 1;
	int ret = lp->fl->erase(lp->fl->ctx, base(lp, page), LOG_PAGE_SIZE);

	if (ret) {
		return ret;
	}
	lp->st.head = page;
	if (page - lp->st.oldest >= lp->pages) {
		lp->st.oldest = page - lp->pages + 1;
	}
	lp->off = 0;
	lp->fl->persist(lp->fl->ctx, &lp->st);
	return 0;
}

int logpack_close(struct logpack *lp)
{
	if (!lp->open) {
		return 0;
	}
	lp->open = false;
	return write_w1(lp, base(lp, lp->st.head) + lp->blk_off, lp->blk_seq, lp->blk_n);
}

/* Walk the head page: close an open block, find where writing resumes. */
static int scan_head(struct logpack *lp)
{
	uint32_t b = base(lp, lp->st.head);
	uint32_t i = 0;
	int ret = 0;

	while (i + MIN_BLOCK <= LOG_PAGE_SIZE) {
		uint8_t h[LOG_HEADER_SIZE];

		ret = rd(lp, b + i, h, sizeof(h));
		if (ret) {
			return ret;
		}
		uint32_t t0 = get32(h), w1 = get32(&h[4]);
		uint8_t n;

		if (t0 == ERASED32) {
			break;
		}
		if (w1 == ERASED32) {
			uint8_t rec[LOG_RECORD_SIZE];

			for (n = 0; n < LOG_BLOCK_MAX &&
				    i + LOG_HEADER_SIZE + (n + 1U) * LOG_RECORD_SIZE <= LOG_PAGE_SIZE;
			     n++) {
				ret = rd(lp, b + i + LOG_HEADER_SIZE + n * LOG_RECORD_SIZE, rec, sizeof(rec));
				if (ret) {
					return ret;
				}
				if (!log_record_written(rec)) {
					break;
				}
			}
			if (n == 0) {
				break; /* cannot happen: the first record goes before the time */
			}
			ret = mend_rec(lp, b + i + LOG_HEADER_SIZE + (n - 1U) * LOG_RECORD_SIZE);
			ret = ret ? ret : write_w1(lp, b + i, lp->st.seq++, n);
			if (ret) {
				return ret;
			}
		} else {
			n = h[7];
			lp->st.seq = (uint16_t)((h[4] | (h[5] << 8)) + 1);
			if (h[6] != LOG_FORMAT || n == 0) {
				break;
			}
		}
		i += LOG_HEADER_SIZE + (uint32_t)n * LOG_RECORD_SIZE;
	}
	lp->off = (uint16_t)i;
	/* anything written past the last block (a record cut off by a reset) ends the page */
	if (i + MIN_BLOCK > LOG_PAGE_SIZE || !erased(lp, b + i, LOG_PAGE_SIZE - i)) {
		lp->off = LOG_PAGE_SIZE;
	}
	return 0;
}

int logpack_mount(struct logpack *lp, const struct logpack_flash *fl, uint32_t pages,
		  const struct logpack_state *saved)
{
	memset(lp, 0, sizeof(*lp));
	lp->fl = fl;
	lp->pages = pages;
	if (pages < 2) {
		return -EINVAL;
	}
	if (saved && saved->head - saved->oldest < pages) {
		lp->st = *saved;
		return scan_head(lp);
	}
	int ret = fl->erase(fl->ctx, 0, pages * LOG_PAGE_SIZE);

	if (ret == 0) {
		fl->persist(fl->ctx, &lp->st);
	}
	return ret;
}

int logpack_append(struct logpack *lp, uint32_t t, const uint8_t rec[LOG_RECORD_SIZE])
{
	int ret;

	if (lp->open) {
		bool fits = lp->off + LOG_RECORD_SIZE <= LOG_PAGE_SIZE;
		bool next = t == lp->blk_t0 + (uint32_t)LOG_EPOCH_S * lp->blk_n;

		if (fits && next && lp->blk_n < LOG_BLOCK_MAX) {
			ret = put_rec(lp, base(lp, lp->st.head) + lp->off, rec);
			if (ret == 0) {
				lp->off += LOG_RECORD_SIZE;
				lp->blk_n++;
			} else {
				lp->off = LOG_PAGE_SIZE; /* close this block and move on next time */
			}
			return ret;
		}
		ret = logpack_close(lp);
		if (ret) {
			return ret;
		}
	}
	if (lp->off + MIN_BLOCK > LOG_PAGE_SIZE) {
		ret = next_page(lp);
		if (ret) {
			return ret;
		}
	}
	uint32_t at = base(lp, lp->st.head) + lp->off;
	uint8_t t0[4];

	/* record first, then the time: a reset in between leaves no empty block */
	put32(t0, t);
	ret = put_rec(lp, at + LOG_HEADER_SIZE, rec);
	ret = ret ? ret : wr(lp, at, t0, sizeof(t0));
	if (ret) {
		lp->off = LOG_PAGE_SIZE; /* this page is spoiled from here: move on next time */
		return ret;
	}
	lp->open = true;
	lp->blk_off = lp->off;
	lp->blk_t0 = t;
	lp->blk_n = 1;
	lp->blk_seq = lp->st.seq++;
	lp->off += MIN_BLOCK;
	return 0;
}

int logpack_erase_all(struct logpack *lp)
{
	int ret = lp->fl->erase(lp->fl->ctx, 0, lp->pages * LOG_PAGE_SIZE);

	lp->open = false;
	lp->st.head += 1;
	lp->st.oldest = lp->st.head;
	lp->off = 0;
	lp->fl->persist(lp->fl->ctx, &lp->st);
	return ret;
}

int logpack_read_page(struct logpack *lp, uint32_t page, uint16_t off, void *buf, size_t len)
{
	if (page < lp->st.oldest || page > lp->st.head || off + len > LOG_PAGE_SIZE) {
		return -ERANGE;
	}
	return rd(lp, base(lp, page) + off, buf, len);
}

void logpack_snapshot(const struct logpack *lp, struct logpack_snap *s)
{
	s->head = lp->st.head;
	s->off = lp->off < LOG_PAGE_SIZE ? lp->off : (uint16_t)LOG_PAGE_SIZE;
	s->w1_off = lp->open ? (uint16_t)(lp->blk_off + 4U) : (uint16_t)LOG_PAGE_SIZE;
}

int logpack_read_snap(struct logpack *lp, const struct logpack_snap *s, uint32_t page,
		      uint16_t off, void *buf, size_t len)
{
	int ret = logpack_read_page(lp, page, off, buf, len);
	uint8_t *b = buf;

	if (ret || page != s->head) {
		return ret;
	}
	for (size_t i = 0; i < len; i++) {
		size_t at = off + i;

		if (at >= s->off || (at >= s->w1_off && at < s->w1_off + 4U)) {
			b[i] = 0xFF;
		}
	}
	return 0;
}
