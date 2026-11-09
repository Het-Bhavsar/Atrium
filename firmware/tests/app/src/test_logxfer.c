/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Whole log syncs (docs/ble_gatt.md 7): logxfer.c over logpack.c on a RAM flash, against a
 * phone that does what the app does (app/android-ios: PodHost.swift / PodHost.kt collect
 * Log Data into 4 KB pages and keep the whole ones from the first page up to the first gap;
 * pod_link.dart decodes them with decodeBlocks, rebases the last uptime run and sets the
 * cursor to the last whole page). Every epoch the phone decodes must be one the pod logged,
 * byte for byte, and a sync from the cursor must bring the rest.
 */
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

#include "logpack.h"
#include "logxfer.h"

#define PAGES 8
#define MAXREF 4000

static uint8_t mem[PAGES * LOG_PAGE_SIZE];
static bool overwrite;
static struct logpack_state saved;
static struct logpack lp;

static int f_read(void *ctx, uint32_t off, void *buf, size_t len)
{
	ARG_UNUSED(ctx);
	memcpy(buf, &mem[off], len);
	return 0;
}

static int f_write(void *ctx, uint32_t off, const void *buf, size_t len)
{
	ARG_UNUSED(ctx);
	for (size_t i = 0; i < len; i++) {
		overwrite |= mem[off + i] != 0xFF;
		mem[off + i] = ((const uint8_t *)buf)[i];
	}
	return 0;
}

static int f_erase(void *ctx, uint32_t off, size_t len)
{
	ARG_UNUSED(ctx);
	memset(&mem[off], 0xFF, len);
	return 0;
}

static void f_persist(void *ctx, const struct logpack_state *st)
{
	ARG_UNUSED(ctx);
	saved = *st;
}

static const struct logpack_flash fl = {f_read, f_write, f_erase, f_persist, NULL};

/* the pod's page source, as logstore.c gives it */
static bool unmasked; /* the read before the snapshot existed: the negative control */

static void s_range(void *ctx, uint32_t *o, uint32_t *h)
{
	ARG_UNUSED(ctx);
	*o = lp.st.oldest;
	*h = lp.st.head;
}

static void s_snap(void *ctx, struct logpack_snap *s)
{
	ARG_UNUSED(ctx);
	logpack_snapshot(&lp, s);
}

static int s_read(void *ctx, const struct logpack_snap *s, uint32_t page, uint16_t off,
		  void *buf, size_t len)
{
	ARG_UNUSED(ctx);
	return unmasked ? logpack_read_page(&lp, page, off, buf, len)
			: logpack_read_snap(&lp, s, page, off, buf, len);
}

static const struct logxfer_src src = {s_range, s_snap, s_read, NULL};
static struct logxfer x;

/* every record logged: its time and bytes */
static struct {
	uint32_t t;
	uint8_t rec[LOG_RECORD_SIZE];
} ref[MAXREF];
static int nref;
static uint32_t next_t;
static uint32_t rng = 12345;

static void log_one(void)
{
	uint8_t rec[LOG_RECORD_SIZE];

	zassert_true(nref < MAXREF);
	for (int i = 0; i < LOG_RECORD_SIZE; i++) {
		rng = rng * 1103515245U + 12345U;
		rec[i] = (uint8_t)(rng >> 16);
	}
	rec[0] = 40 + rec[0] % 150; /* never 0xFF: a torn record shows */
	rec[11] &= 0x0F;
	zassert_ok(logpack_append(&lp, next_t, rec));
	ref[nref].t = next_t;
	memcpy(ref[nref].rec, rec, sizeof(rec));
	nref++;
	next_t += LOG_EPOCH_S;
}

static void log_n(int n, int gap_every)
{
	for (int i = 0; i < n; i++) {
		log_one();
		if (gap_every && (nref % gap_every) == 0) {
			next_t += 600; /* a Plinth break: a new block */
		}
	}
}

/* ---- the phone ---- */

#define CPAGES 12

static struct central {
	uint32_t first;
	int expected;
	uint8_t status;
	bool got_rsp;
	bool ended;
	uint8_t end_reason;
	uint32_t end_value;
	bool data_after_end;
	uint8_t page[CPAGES][LOG_PAGE_SIZE];
	bool complete[CPAGES];
	int packets;
} ph;

static uint8_t buf[CPAGES * LOG_PAGE_SIZE];

static void ph_begin(uint32_t from)
{
	memset(&ph, 0, sizeof(ph));
	memset(ph.page, 0xFF, sizeof(ph.page));
	ph.first = from;
}

static void ph_indication(const uint8_t v[LOGX_IND_LEN])
{
	uint32_t val = v[2] | v[3] << 8 | v[4] << 16 | (uint32_t)v[5] << 24;

	if (v[0] == 0x81) {
		ph.got_rsp = true;
		ph.status = v[1];
		ph.expected = (int)val;
	} else if (v[0] == LOGX_END) {
		zassert_false(ph.ended, "one end indication per read");
		ph.ended = true;
		ph.end_reason = v[1];
		ph.end_value = val;
	}
}

static void ph_data(const uint8_t *p, size_t n)
{
	uint32_t page = p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
	uint16_t off = p[4] | p[5] << 8;
	size_t len = n - LOGX_DATA_HDR;

	ph.data_after_end |= ph.ended;
	ph.packets++;
	zassert_true(page >= ph.first && page - ph.first < CPAGES);
	zassert_true(off % 4 == 0 && len % 4 == 0 && off + len <= LOG_PAGE_SIZE);
	memcpy(&ph.page[page - ph.first][off], &p[LOGX_DATA_HDR], len);
	if (off + len >= LOG_PAGE_SIZE) {
		ph.complete[page - ph.first] = true;
	}
}

/* finishSync(): the whole pages from the first up to the first gap, into buf */
static int ph_pages(void)
{
	int n = 0;

	while (n < CPAGES && ph.complete[n]) {
		memcpy(&buf[n * LOG_PAGE_SIZE], ph.page[n], LOG_PAGE_SIZE);
		n++;
	}
	return n;
}

/* PodLink.nextCursor / syncFrom */
static uint32_t next_cursor(uint32_t first, int n, uint32_t cursor)
{
	return n == 0 ? cursor : first + (uint32_t)n - 1;
}

static uint32_t sync_from(uint32_t cursor)
{
	return cursor >= lp.st.oldest && cursor <= lp.st.head ? cursor : lp.st.oldest;
}

static void request(uint32_t from, uint32_t count)
{
	uint8_t req[9] = {LOGX_OP_READ};
	uint8_t rsp[LOGX_IND_LEN];

	sys_put_le32(from, &req[1]);
	sys_put_le32(count, &req[5]);
	logxfer_control(&x, &src, req, sizeof(req), rsp);
	ph_indication(rsp);
}

/* The sender (gatt_atrium.c log_xfer_fn): packets, then the end indication. `between` runs
 * after each packet; `stop_after` cuts the link after that many packets (-1: never). */
static void pump(uint16_t per, void (*between)(int k), int stop_after)
{
	uint8_t pkt[LOGX_DATA_HDR + LOGX_DATA_MAX];
	uint8_t end[LOGX_IND_LEN];

	for (int k = 0;; k++) {
		if (k == stop_after) {
			logxfer_cancel(&x);
			return;
		}
		size_t n = logxfer_packet(&x, &src, per, pkt);

		if (n == 0) {
			break;
		}
		ph_data(pkt, n);
		logxfer_sent(&x, n);
		if (between) {
			between(k);
		}
	}
	if (logxfer_end(&x, end)) {
		ph_indication(end);
		logxfer_end_sent(&x);
	}
	zassert_false(logxfer_end(&x, end));
}

/* ---- decodeBlocks (log_record.dart), strict ---- */

static struct ep {
	uint32_t t;
	bool up;
	uint8_t rec[LOG_RECORD_SIZE];
} got[MAXREF];
static int ngot;

static int decode(const uint8_t *b, int pages)
{
	ngot = 0;
	for (int p = 0; p < pages; p++) {
		const uint8_t *pg = &b[p * LOG_PAGE_SIZE];
		int i = 0;

		while (i + LOG_HEADER_SIZE + LOG_RECORD_SIZE <= LOG_PAGE_SIZE) {
			uint32_t t0 = sys_get_le32(&pg[i]);
			uint32_t w1 = sys_get_le32(&pg[i + 4]);
			int n = 0;

			if (t0 == 0xFFFFFFFF) {
				break;
			}
			if (w1 == 0xFFFFFFFF) {
				while (i + LOG_HEADER_SIZE + (n + 1) * LOG_RECORD_SIZE <= LOG_PAGE_SIZE &&
				       (pg[i + LOG_HEADER_SIZE + n * LOG_RECORD_SIZE + 11] >> 4) == 0) {
					n++;
				}
			} else {
				n = pg[i + 7];
				if (pg[i + 6] != LOG_FORMAT || n == 0 ||
				    i + LOG_HEADER_SIZE + n * LOG_RECORD_SIZE > LOG_PAGE_SIZE) {
					return -1; /* LogFormatException: bad block */
				}
			}
			i += LOG_HEADER_SIZE;
			for (int k = 0; k < n; k++) {
				if (pg[i + 11] >> 4) {
					return -2; /* reserved bits set */
				}
				got[ngot].t = (t0 & ~LOG_TIME_UPTIME) + LOG_EPOCH_S * k;
				got[ngot].up = (t0 & LOG_TIME_UPTIME) != 0;
				memcpy(got[ngot].rec, &pg[i], LOG_RECORD_SIZE);
				ngot++;
				i += LOG_RECORD_SIZE;
			}
			if (w1 == 0xFFFFFFFF) {
				break;
			}
		}
	}
	return ngot;
}

static int find_ref(uint32_t t)
{
	for (int i = 0; i < nref; i++) {
		if ((ref[i].t & ~LOG_TIME_UPTIME) == t) {
			return i;
		}
	}
	return -1;
}

/* the phone's store: unique by time */
static bool have[MAXREF];

static int store_add(void)
{
	int added = 0;

	for (int i = 0; i < ngot; i++) {
		int r = find_ref(got[i].t);

		zassert_true(r >= 0, "epoch at %u was never logged", got[i].t);
		zassert_mem_equal(got[i].rec, ref[r].rec, LOG_RECORD_SIZE, "epoch at %u: torn",
				  got[i].t);
		if (!have[r]) {
			have[r] = true;
			added++;
		}
	}
	return added;
}

/* epochs from `from_ref` on that the pod still holds are all in the store */
static void assert_store_has_held(void)
{
	uint8_t pg[LOG_PAGE_SIZE];
	int held = 0;

	/* what the pod holds, read directly */
	for (uint32_t p = lp.st.oldest; p <= lp.st.head; p++) {
		zassert_ok(logpack_read_page(&lp, p, 0, pg, sizeof(pg)));
		memcpy(&buf[(p - lp.st.oldest) * LOG_PAGE_SIZE], pg, sizeof(pg));
	}
	zassert_true(decode(buf, (int)(lp.st.head - lp.st.oldest + 1)) >= 0);
	for (int i = 0; i < ngot; i++) {
		int r = find_ref(got[i].t);

		zassert_true(r >= 0 && have[r], "held epoch at %u not synced", got[i].t);
		held++;
	}
	zassert_true(held > 0);
}

/* One sync as pod_link.dart runs it; returns the epochs added, updates the cursor. */
static int sync(uint32_t *cursor, uint16_t per, void (*between)(int k), int stop_after)
{
	uint32_t from = sync_from(*cursor);

	ph_begin(from);
	request(from, 0);
	zassert_true(ph.got_rsp);
	zassert_equal(ph.status, LOGX_OK);
	pump(per, between, stop_after);

	int n = ph_pages();

	zassert_true(decode(buf, n) >= 0, "decodeBlocks threw");
	*cursor = next_cursor(from, n, *cursor);
	return store_add();
}

static void before(void *f)
{
	ARG_UNUSED(f);
	memset(mem, 0x5A, sizeof(mem));
	overwrite = false;
	unmasked = false;
	zassert_ok(logpack_mount(&lp, &fl, PAGES, NULL));
	memset(&x, 0, sizeof(x));
	memset(have, 0, sizeof(have));
	nref = 0;
	next_t = 1762560000; /* 2025-11-08, Unix */
}

ZTEST_SUITE(logxfer, NULL, NULL, before, NULL, NULL);

/* 1a. Oldest to head, the head page partial; then the head page again from the cursor */
ZTEST(logxfer, test_full_sync_then_resume_from_head)
{
	uint32_t cursor = 0;

	log_n(3 * 340 + 100, 97);
	zassert_true(lp.st.head >= 3);
	zassert_equal(sync(&cursor, 236, NULL, -1), nref);
	zassert_equal(ph.expected, (int)(lp.st.head + 1), "pages to send");
	zassert_true(ph.ended && ph.end_reason == LOGX_END_DONE);
	zassert_equal(ph.end_value, lp.st.head + 1);
	zassert_false(ph.data_after_end, "the end indication follows the last Log Data");
	zassert_equal(cursor, lp.st.head, "cursor = the head page, read again next time");

	int before_n = nref;
	uint32_t c0 = cursor;

	log_n(50, 0);
	zassert_equal(sync(&cursor, 236, NULL, -1), nref - before_n, "only the new epochs");
	zassert_equal(ph.expected, (int)(lp.st.head + 1 - c0));
	assert_store_has_held();
	zassert_false(overwrite);
}

/* 1b. Records keep arriving while the head page streams, at every alignment and MTU */
static void append_each(int k)
{
	ARG_UNUSED(k);
	log_one();
}

static int torn_runs;

static void run_alignments(void)
{
	static const uint16_t pers[] = {12, 236, 240};

	bool um = unmasked;

	torn_runs = 0;
	for (size_t m = 0; m < ARRAY_SIZE(pers); m++) {
		for (int pre = 0; pre < 40; pre++) {
			before(NULL);
			unmasked = um;
			log_n(pre + 1, 0);

			uint32_t from = lp.st.head;

			ph_begin(from);
			request(from, 0);
			pump(pers[m], append_each, -1);

			int n = ph_pages();
			int d = decode(buf, n);

			if (d < 0) {
				torn_runs++;
				continue;
			}
			for (int i = 0; i < ngot; i++) {
				int r = find_ref(got[i].t);

				if (r < 0 || memcmp(got[i].rec, ref[r].rec, LOG_RECORD_SIZE)) {
					torn_runs++;
					break;
				}
			}
		}
	}
}

ZTEST(logxfer, test_appends_during_sync_never_tear)
{
	run_alignments();
	zassert_equal(torn_runs, 0, "a record read half-written");
}

ZTEST(logxfer, test_unmasked_head_read_tears)
{
	/* the read before the snapshot: the same runs tear, so the test above can tell */
	unmasked = true;
	run_alignments();
	unmasked = false;
	zassert_true(torn_runs > 0, "the negative control found no torn record");
}

/* 1b. The open block closes mid-read (lift onto the Plinth): its count word is masked, the
 * phone counts by byte 11 within the snapshot, and the next sync gets the rest */
static void close_at_3(int k)
{
	if (k == 3) {
		zassert_ok(logpack_close(&lp));
		next_t += 600;
		log_n(5, 0);
	}
}

ZTEST(logxfer, test_block_closes_mid_sync)
{
	uint32_t cursor = 0;

	log_n(30, 0);
	cursor = lp.st.head;
	int n0 = sync(&cursor, 12, close_at_3, -1);

	zassert_equal(n0, 30, "the 30 epochs of the snapshot, not the 5 after it");
	zassert_equal(sync(&cursor, 12, NULL, -1), 5);
}

/* 1c. The ring drops the oldest page while it is being sent */
static void wrap_at_2(int k)
{
	if (k == 2) {
		uint32_t h = lp.st.head;

		while (lp.st.head == h) {
			log_one();
		}
	}
}

ZTEST(logxfer, test_ring_wraps_during_sync)
{
	uint32_t cursor = 0;

	log_n(8 * 340 - 20, 0); /* full ring, head page nearly full */
	zassert_equal(lp.st.oldest, 0);
	zassert_equal(lp.st.head, 7);

	int n = sync(&cursor, 236, wrap_at_2, -1);

	zassert_true(ph.ended && ph.end_reason == LOGX_END_DROPPED);
	zassert_equal(ph.end_value, 0, "no whole page arrived");
	zassert_equal(n, 0);
	zassert_equal(cursor, 0, "cursor kept");
	zassert_equal(lp.st.oldest, 1);

	/* the next sync starts at the new oldest page and gets everything held */
	n = sync(&cursor, 236, NULL, -1);
	zassert_true(ph.ended && ph.end_reason == LOGX_END_DONE);
	zassert_true(n > 0);
	assert_store_has_held();
}

ZTEST(logxfer, test_ring_wraps_after_oldest_sent)
{
	log_n(8 * 340 - 20, 0);
	/* 18 packets of 236 bytes = page 0 sent whole; then the ring drops page 0 */
	ph_begin(0);
	request(0, 0);

	static uint8_t pkt[LOGX_DATA_HDR + LOGX_DATA_MAX];
	size_t len;
	int k = 0;

	while ((len = logxfer_packet(&x, &src, 236, pkt)) != 0) {
		ph_data(pkt, len);
		logxfer_sent(&x, len);
		if (++k == 18) {
			wrap_at_2(2);
		}
	}
	uint8_t end[LOGX_IND_LEN];

	zassert_true(logxfer_end(&x, end));
	ph_indication(end);
	zassert_equal(ph.end_reason, LOGX_END_DONE, "the dropped page had already gone");
	zassert_equal(ph_pages(), 8, "pages 0 to 7 of the snapshot");
}

/* 1d. The phone leaves mid-sync; the next connect resumes from the cursor */
ZTEST(logxfer, test_disconnect_mid_sync_resumes)
{
	uint32_t cursor = 0;

	log_n(4 * 340 + 10, 0);
	int a = sync(&cursor, 236, NULL, 2 * 18 + 9); /* two pages and a half */

	zassert_false(ph.ended, "no end indication after the link went");
	zassert_false(x.active);
	zassert_equal(cursor, 1, "last whole page");
	zassert_true(a > 0 && a < nref);

	log_n(20, 0);
	int b = sync(&cursor, 236, NULL, -1);

	zassert_equal(a + b, nref, "every epoch once");
	zassert_equal(cursor, lp.st.head);
	assert_store_has_held();
}

/* 1e. A second read while one runs, abort, and bad requests */
ZTEST(logxfer, test_busy_abort_and_bad_requests)
{
	uint8_t rsp[LOGX_IND_LEN];
	uint8_t pkt[LOGX_DATA_HDR + LOGX_DATA_MAX];
	uint8_t end[LOGX_IND_LEN];

	log_n(2 * 340 + 5, 0);
	ph_begin(0);
	request(0, 0);
	zassert_equal(ph.status, LOGX_OK);
	size_t n = logxfer_packet(&x, &src, 236, pkt);

	ph_data(pkt, n);
	logxfer_sent(&x, n);
	request(1, 0);
	zassert_equal(ph.status, LOGX_BUSY, "status 2");
	zassert_true(x.active && x.first == 0, "the first read goes on");
	pump(236, NULL, -1);
	zassert_equal(ph_pages(), 3);
	zassert_equal(ph.end_value, 3);

	/* abort: 0x82 ok, no end indication */
	request(0, 0);
	n = logxfer_packet(&x, &src, 236, pkt);
	logxfer_sent(&x, n);
	logxfer_control(&x, &src, (const uint8_t[]){LOGX_OP_ABORT}, 1, rsp);
	zassert_equal(rsp[0], 0x82);
	zassert_equal(rsp[1], LOGX_OK);
	zassert_false(logxfer_end(&x, end));
	zassert_equal(logxfer_packet(&x, &src, 236, pkt), 0);

	/* count limits the pages */
	ph_begin(0);
	request(0, 2);
	zassert_equal(ph.expected, 2);
	pump(236, NULL, -1);
	zassert_equal(ph_pages(), 2);
	zassert_equal(ph.end_value, 2);

	/* a read done but its end not yet sent: a new read drops that end */
	request(0, 1);
	while ((n = logxfer_packet(&x, &src, 240, pkt)) != 0) {
		logxfer_sent(&x, n);
	}
	zassert_true(logxfer_end(&x, end));
	request(1, 1);
	zassert_equal(ph.status, LOGX_OK);
	zassert_false(logxfer_end(&x, end), "a stale end would cut the new read short");
	logxfer_cancel(&x);

	request(lp.st.head + 1, 0);
	zassert_equal(ph.status, LOGX_RANGE, "past the head");
	logxfer_control(&x, &src, (const uint8_t[]){LOGX_OP_READ, 0, 0}, 3, rsp);
	zassert_equal(rsp[0], 0x81);
	zassert_equal(rsp[1], LOGX_BAD);
	logxfer_control(&x, &src, (const uint8_t[]){0x07}, 1, rsp);
	zassert_equal(rsp[0], 0x87);
	zassert_equal(rsp[1], LOGX_BAD);

	/* below the oldest page */
	log_n(6 * 340, 0);
	zassert_true(lp.st.oldest > 0);
	request(0, 0);
	zassert_equal(ph.status, LOGX_RANGE);
}

/* A notification that found no buffer is built again, the same bytes */
ZTEST(logxfer, test_no_buffer_resends_same_packet)
{
	uint8_t a[LOGX_DATA_HDR + LOGX_DATA_MAX], b[LOGX_DATA_HDR + LOGX_DATA_MAX];

	log_n(100, 0);
	request(0, 0);
	size_t n1 = logxfer_packet(&x, &src, 240, a);
	size_t n2 = logxfer_packet(&x, &src, 240, b);

	zassert_equal(n1, n2);
	zassert_mem_equal(a, b, n1);
	logxfer_sent(&x, n1);
	n2 = logxfer_packet(&x, &src, 240, b);
	zassert_equal(sys_get_le16(&b[4]), 240, "then the next piece");
}

/* 3. Uptime epochs (clock lost at a reboot) then Unix ones; the app's rebaseLastRun */
static int rebase_last_run(uint32_t uptime_now, uint32_t unix_now)
{
	int end = ngot - 1;

	while (end >= 0 && !got[end].up) {
		end--;
	}
	if (end < 0 || got[end].t > uptime_now) {
		return 0;
	}
	int start = end;

	while (start > 0 && got[start - 1].up && got[start - 1].t < got[start].t) {
		start--;
	}
	for (int i = start; i <= end; i++) {
		got[i].t += unix_now - uptime_now;
		got[i].up = false;
	}
	return end - start + 1;
}

ZTEST(logxfer, test_uptime_epochs_rebase_after_reboot)
{
	/* boot 1, clock set: Unix epochs */
	log_n(40, 0);
	/* reboot: the open block is closed on mount; the clock is lost */
	struct logpack_state st = saved;

	st.head = lp.st.head;
	st.oldest = lp.st.oldest;
	st.seq = lp.st.seq;
	zassert_ok(logpack_mount(&lp, &fl, PAGES, &st));
	uint32_t up0 = 95; /* first epoch 95 s after boot */
	uint32_t unix_boot = next_t + 3600;

	next_t = LOG_TIME_UPTIME | up0;
	log_n(20, 0);
	/* the phone connects: the clock reads uptime, then is set */
	uint32_t uptime_now = up0 + 20 * LOG_EPOCH_S + 7;
	uint32_t unix_now = unix_boot + uptime_now;

	next_t = (unix_now / LOG_EPOCH_S + 1) * LOG_EPOCH_S; /* epoch_next_t0 after the set */
	log_n(10, 0);

	ph_begin(0);
	request(0, 0);
	pump(236, NULL, -1);
	int n = ph_pages();

	zassert_equal(decode(buf, n), 70);
	zassert_true(got[40].up && got[59].up && !got[39].up && !got[60].up);
	zassert_equal(got[40].t, up0, "bit 31 stripped, uptime seconds");
	zassert_equal(rebase_last_run(uptime_now, unix_now), 20, "the current boot's run");
	for (int i = 40; i < 60; i++) {
		zassert_equal(got[i].t, unix_boot + up0 + (uint32_t)(i - 40) * LOG_EPOCH_S);
	}
	zassert_true(got[59].t < got[60].t, "rebased epochs come before the Unix ones");
}
