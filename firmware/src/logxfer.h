/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The log download of docs/ble_gatt.md section 7, without the radio: Log Control requests,
 * Log Data packets and the end-of-read indication. gatt_atrium.c sends what it builds.
 * Plain C over a page source, so the host tests run whole syncs against logpack.c.
 *  - A read sends whole pages, from a page to the head as it stood at the request (a
 *    logpack snapshot: records that arrive during the read wait for the next one).
 *  - A packet is built, then only counted once the radio took it (logxfer_sent), so a
 *    notification that found no buffer is built again.
 *  - When the last page has gone, or the page being sent left the ring, one end
 *    indication is due: 0x90, reason, u32 whole pages sent.
 */
#ifndef ATRIUM_LOGXFER_H_
#define ATRIUM_LOGXFER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "logpack.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { LOGX_OP_READ = 0x01, LOGX_OP_ABORT = 0x02 };
/* response status */
enum { LOGX_OK = 0, LOGX_BAD = 1, LOGX_BUSY = 2, LOGX_RANGE = 3 };
/* the end indication: first byte, then the reason */
#define LOGX_END 0x90
enum { LOGX_END_DONE = 0, LOGX_END_DROPPED = 1 };

#define LOGX_IND_LEN  6 /* u8 op | 0x80 (or 0x90), u8 status (or reason), u32 value */
#define LOGX_DATA_HDR 6 /* u32 page, u16 offset */
#define LOGX_DATA_MAX 240

struct logxfer_src {
	void (*range)(void *ctx, uint32_t *oldest, uint32_t *head);
	void (*snapshot)(void *ctx, struct logpack_snap *s);
	int (*read)(void *ctx, const struct logpack_snap *s, uint32_t page, uint16_t off,
		    void *buf, size_t len);
	void *ctx;
};

struct logxfer {
	bool active;
	bool end_due;
	uint8_t reason;
	uint32_t first; /* first page of the read */
	uint32_t page;  /* page being sent */
	uint16_t off;   /* next byte in it */
	uint32_t end;   /* first page not sent */
	struct logpack_snap snap;
};

/* A Log Control write; `rsp` is the indication to send back. A new read drops an end
 * indication not yet sent, so a stale one never ends the next read. */
void logxfer_control(struct logxfer *x, const struct logxfer_src *src, const uint8_t *req,
		     size_t len, uint8_t rsp[LOGX_IND_LEN]);

/* The next Log Data packet with up to `per` page bytes (a multiple of 4), into `pkt`
 * (LOGX_DATA_HDR + per bytes): its length, or 0 when the read is over or idle. */
size_t logxfer_packet(struct logxfer *x, const struct logxfer_src *src, uint16_t per,
		      uint8_t *pkt);

/* The packet of `len` bytes that logxfer_packet() built went out. */
void logxfer_sent(struct logxfer *x, size_t len);

/* True when the end indication is due; fills `out`. logxfer_end_sent() once it went out. */
bool logxfer_end(const struct logxfer *x, uint8_t out[LOGX_IND_LEN]);
void logxfer_end_sent(struct logxfer *x);

/* The phone left: no packets, no end indication. */
void logxfer_cancel(struct logxfer *x);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_LOGXFER_H_ */
