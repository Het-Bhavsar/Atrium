/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "logxfer.h"

#include <string.h>

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void ind(uint8_t out[LOGX_IND_LEN], uint8_t op, uint8_t status, uint32_t value)
{
	out[0] = op;
	out[1] = status;
	put32(&out[2], value);
}

void logxfer_control(struct logxfer *x, const struct logxfer_src *src, const uint8_t *req,
		     size_t len, uint8_t rsp[LOGX_IND_LEN])
{
	uint8_t op = len ? req[0] : 0;
	uint32_t oldest, head;

	if (op == LOGX_OP_ABORT) {
		x->active = false;
		x->end_due = false;
		ind(rsp, op | 0x80, LOGX_OK, 0);
		return;
	}
	if (op != LOGX_OP_READ || len != 9) {
		ind(rsp, op | 0x80, LOGX_BAD, 0);
		return;
	}
	if (x->active) {
		ind(rsp, op | 0x80, LOGX_BUSY, 0);
		return;
	}
	uint32_t from = get32(&req[1]);
	uint32_t count = get32(&req[5]);

	src->range(src->ctx, &oldest, &head);
	if (from < oldest || from > head) {
		ind(rsp, op | 0x80, LOGX_RANGE, 0);
		return;
	}
	src->snapshot(src->ctx, &x->snap);
	head = x->snap.head;
	x->first = from;
	x->page = from;
	x->off = 0;
	x->end = (count == 0 || count > head + 1 - from) ? head + 1 : from + count;
	x->active = true;
	x->end_due = false;
	ind(rsp, op | 0x80, LOGX_OK, x->end - from);
}

static void finish(struct logxfer *x, uint8_t reason)
{
	x->active = false;
	x->end_due = true;
	x->reason = reason;
}

size_t logxfer_packet(struct logxfer *x, const struct logxfer_src *src, uint16_t per,
		      uint8_t *pkt)
{
	if (!x->active) {
		return 0;
	}
	if (x->page >= x->end) {
		finish(x, LOGX_END_DONE);
		return 0;
	}
	per -= per % 4;
	if (per > LOGX_DATA_MAX) {
		per = LOGX_DATA_MAX;
	}
	if (per == 0) {
		return 0;
	}
	uint16_t n = per < LOG_PAGE_SIZE - x->off ? per : (uint16_t)(LOG_PAGE_SIZE - x->off);

	put32(pkt, x->page);
	pkt[4] = (uint8_t)x->off;
	pkt[5] = (uint8_t)(x->off >> 8);
	if (src->read(src->ctx, &x->snap, x->page, x->off, &pkt[LOGX_DATA_HDR], n)) {
		finish(x, LOGX_END_DROPPED); /* the page left the ring while sending */
		return 0;
	}
	return LOGX_DATA_HDR + n;
}

void logxfer_sent(struct logxfer *x, size_t len)
{
	if (!x->active || len <= LOGX_DATA_HDR) {
		return;
	}
	x->off += (uint16_t)(len - LOGX_DATA_HDR);
	if (x->off >= LOG_PAGE_SIZE) {
		x->off = 0;
		x->page++;
	}
	if (x->page >= x->end) {
		finish(x, LOGX_END_DONE);
	}
}

bool logxfer_end(const struct logxfer *x, uint8_t out[LOGX_IND_LEN])
{
	if (!x->end_due) {
		return false;
	}
	ind(out, LOGX_END, x->reason, x->page - x->first);
	return true;
}

void logxfer_end_sent(struct logxfer *x)
{
	x->end_due = false;
}

void logxfer_cancel(struct logxfer *x)
{
	x->active = false;
	x->end_due = false;
}
