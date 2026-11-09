/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The page writer on a RAM flash that allows each byte to be written once after an erase,
 * as the nRF52 does. The pages must equal logrec.pack_pages() for the same runs, and a
 * reset at any write must leave a log that logrec.decode_blocks() reads.
 */
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/ztest.h>

#include "log_vec.h"
#include "logpack.h"

#define PAGES 8

static uint8_t mem[PAGES * LOG_PAGE_SIZE];
static int writes_left; /* < 0: no reset */
static int cut_bytes;   /* the failing write gets this many bytes (whole words) in first */
static bool overwrite;
static struct logpack_state saved;
static int persists;
static struct logpack lp;

static int f_read(void *ctx, uint32_t off, void *buf, size_t len)
{
	ARG_UNUSED(ctx);
	memcpy(buf, &mem[off], len);
	return 0;
}

static int f_write(void *ctx, uint32_t off, const void *buf, size_t len)
{
	const uint8_t *b = buf;

	ARG_UNUSED(ctx);
	if (writes_left == 0) {
		return -EIO;
	}
	size_t n = len;

	if (writes_left > 0 && --writes_left == 0) {
		n = MIN((size_t)cut_bytes, len); /* the reset lands inside this write */
	}
	for (size_t i = 0; i < n; i++) {
		overwrite |= mem[off + i] != 0xFF;
		mem[off + i] = b[i];
	}
	return n == len ? 0 : -EIO;
}

static int f_erase(void *ctx, uint32_t off, size_t len)
{
	ARG_UNUSED(ctx);
	if (writes_left == 0) {
		return -EIO;
	}
	memset(&mem[off], 0xFF, len);
	return 0;
}

static void f_persist(void *ctx, const struct logpack_state *st)
{
	ARG_UNUSED(ctx);
	saved = *st;
	persists++;
}

static const struct logpack_flash fl = {f_read, f_write, f_erase, f_persist, NULL};

static void before(void *f)
{
	ARG_UNUSED(f);
	memset(mem, 0x5A, sizeof(mem)); /* not erased: mount without state must erase */
	writes_left = -1;
	overwrite = false;
	persists = 0;
	zassert_ok(logpack_mount(&lp, &fl, PAGES, NULL));
}

ZTEST_SUITE(logpack, NULL, NULL, before, NULL, NULL);

static void append(uint32_t t, int k)
{
	struct log_record r = vec_epoch(k);
	uint8_t b[LOG_RECORD_SIZE];

	log_record_pack(&r, b);
	zassert_ok(logpack_append(&lp, t, b), "epoch %d", k);
}

/* logrec.decode_blocks in C: count records and check every block of one page */
struct seen {
	int n;
	uint32_t last_t;
	int open_blocks;
};

static void decode_page(const uint8_t *pg, struct seen *s)
{
	uint32_t i = 0;

	while (i + LOG_HEADER_SIZE + LOG_RECORD_SIZE <= LOG_PAGE_SIZE) {
		uint32_t t0 = sys_get_le32(&pg[i]), w1 = sys_get_le32(&pg[i + 4]);
		int n = 0;

		if (t0 == 0xFFFFFFFF) {
			break;
		}
		if (w1 == 0xFFFFFFFF) {
			while (i + LOG_HEADER_SIZE + (n + 1) * LOG_RECORD_SIZE <= LOG_PAGE_SIZE &&
			       log_record_written(&pg[i + LOG_HEADER_SIZE + n * LOG_RECORD_SIZE])) {
				n++;
			}
			s->open_blocks++;
		} else {
			n = pg[i + 7];
			zassert_equal(pg[i + 6], LOG_FORMAT);
			zassert_true(n > 0 && i + LOG_HEADER_SIZE + n * LOG_RECORD_SIZE <= LOG_PAGE_SIZE,
				     "bad block at %u", i);
		}
		for (int k = 0; k < n; k++) {
			zassert_true(log_record_written(&pg[i + LOG_HEADER_SIZE + k * LOG_RECORD_SIZE]),
				     "reserved bits set in block at %u", i);
		}
		s->n += n;
		s->last_t = t0 + LOG_EPOCH_S * (n - 1);
		i += LOG_HEADER_SIZE + n * LOG_RECORD_SIZE;
		if (w1 == 0xFFFFFFFF) {
			break;
		}
	}
}

static struct seen decode_all(void)
{
	struct seen s = {0};

	for (uint32_t p = lp.st.oldest; p <= lp.st.head; p++) {
		decode_page(&mem[(p % PAGES) * LOG_PAGE_SIZE], &s);
	}
	return s;
}

/* The same three runs as the generator: logrec.pack_pages gives 3 pages, crc32 0x91fffa11 */
ZTEST(logpack, test_pages_match_logrec_pack_pages)
{
	for (int k = 0; k < 300; k++) {
		append(1000 + 30 * k, k);
	}
	for (int k = 300; k < 700; k++) {
		append(1000 + 30 * 300 + 600 + 30 * (k - 300), k);
	}
	for (int k = 700; k < 760; k++) {
		append((1UL << 31 | 5) + 30 * (k - 700), k);
	}
	zassert_ok(logpack_close(&lp));
	zassert_equal(lp.st.head, 2);
	zassert_equal(crc32_ieee(mem, 3 * LOG_PAGE_SIZE), 0x91fffa11);
	zassert_false(overwrite, "a byte was written twice");
	struct seen s = decode_all();

	zassert_equal(s.n, 760);
}

ZTEST(logpack, test_gap_free_page_holds_340)
{
	for (int k = 0; k < 340; k++) {
		append(30 * k, k);
	}
	zassert_equal(lp.st.head, 0);
	append(30 * 340, 340);
	zassert_equal(lp.st.head, 1, "record 341 opens the next page");
}

ZTEST(logpack, test_ring_drops_oldest_page)
{
	for (int k = 0; k < 340 * 9 + 1; k++) {
		append(30 * k, k);
	}
	zassert_equal(lp.st.head, 9);
	zassert_equal(lp.st.oldest, 2, "8 pages held");
	zassert_equal(saved.head, 9, "persisted at each page");
	uint8_t b[4];

	zassert_equal(logpack_read_page(&lp, 1, 0, b, 4), -ERANGE);
	zassert_ok(logpack_read_page(&lp, 2, 0, b, 4));
	zassert_equal(sys_get_le32(b), 30 * 340 * 2, "page 2 starts with epoch 680");
	zassert_equal(logpack_read_page(&lp, 10, 0, b, 4), -ERANGE);
	zassert_false(overwrite);
}

ZTEST(logpack, test_reboot_closes_open_block_and_resumes)
{
	for (int k = 0; k < 10; k++) {
		append(30 * k, k);
	}
	/* reset: the block is open (second header word erased) */
	zassert_equal(sys_get_le32(&mem[4]), 0xFFFFFFFF);
	zassert_ok(logpack_mount(&lp, &fl, PAGES, &saved));
	zassert_equal(mem[7], 10, "closed with 10 records on boot");
	zassert_equal(mem[4] | (mem[5] << 8), 0, "block sequence 0");
	append(30 * 50, 50);
	zassert_ok(logpack_close(&lp));
	zassert_equal(mem[8 + 120 + 4], 1, "the next block is sequence 1");
	zassert_false(overwrite);
	zassert_equal(decode_all().n, 11);
}

/* A reset at every write of a long run, each followed by a reboot and more epochs: the log
 * always decodes, never writes a byte twice, and loses at most the record being written. */
ZTEST(logpack, test_reset_at_any_write)
{
	for (int cut = 1; cut < 600; cut += 7) {
		/* the NVMC writes whole words: a reset leaves 0, 1 or 2 words of a record */
		for (int bytes = 0; bytes < 12; bytes += 4) {
			before(NULL);
			writes_left = cut;
			cut_bytes = bytes;
			int ok = 0;

			for (int k = 0; k < 400; k++) {
				struct log_record r = vec_epoch(k);
				uint8_t b[LOG_RECORD_SIZE];

				log_record_pack(&r, b);
				if (logpack_append(&lp, 30 * k, b)) {
					break;
				}
				ok++;
			}
			writes_left = -1;
			zassert_ok(logpack_mount(&lp, &fl, PAGES, &saved));
			for (int k = 0; k < 50; k++) {
				append(100000 + 30 * k, k);
			}
			zassert_ok(logpack_close(&lp));
			struct seen s = decode_all();

			zassert_false(overwrite, "cut %d/%d: a byte written twice", cut, bytes);
			zassert_equal(s.open_blocks, 0, "cut %d/%d: open block, head %u off %u", cut, bytes, lp.st.head, lp.off);
			zassert_true(s.n >= ok - 1 + 50 && s.n <= ok + 1 + 50, "cut %d/%d: %d of %d",
				     cut, bytes, s.n, ok + 50);
		}
	}
}

ZTEST(logpack, test_erase_all_keeps_counting)
{
	for (int k = 0; k < 400; k++) {
		append(30 * k, k);
	}
	zassert_ok(logpack_erase_all(&lp));
	zassert_equal(lp.st.head, 2);
	zassert_equal(lp.st.oldest, 2);
	zassert_equal(saved.head, 2);
	append(5000, 1);
	zassert_ok(logpack_close(&lp));
	zassert_equal(decode_all().n, 1);
}

