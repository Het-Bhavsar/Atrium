/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "swd_sim.h"

#include <string.h>

enum { PH_OFF, PH_LOCKOUT, PH_WAIT_IDLE, PH_IDLE, PH_REQ, PH_RESP };

void swd_sim_init(struct swd_sim *s)
{
	memset(s, 0, sizeof(*s));
	s->clk = 1;
	s->pull = 1;
	s->present = true;
	s->phase = PH_OFF;
	s->dpidr = 0x2BA01477; /* Cortex-M4 SW-DP */
	s->pwr_acks = true;
	s->idr = NRF52_CTRLAP_IDR;
}

static int line(const struct swd_sim *s)
{
	if (s->host_drives) {
		return s->host_level;
	}
	return s->tgt_drives ? s->tgt_level : s->pull;
}

static uint32_t ap_reg(const struct swd_sim *s, uint8_t a)
{
	return (s->select & 0xF0) | a;
}

static uint32_t ap_read(struct swd_sim *s, uint8_t a)
{
	if (s->select >> 24 != NRF_CTRLAP) {
		return 0; /* AHB-AP: blocked under APPROTECT in this model */
	}
	switch (ap_reg(s, a)) {
	case NRF_CTRLAP_RESET:
		return s->ctrlap_reset;
	case NRF_CTRLAP_APPROTECTSTATUS:
		return 0; /* protected */
	case NRF_CTRLAP_IDR:
		return s->idr;
	default:
		return 0;
	}
}

static void ap_write(struct swd_sim *s, uint8_t a, uint32_t v)
{
	if (s->select >> 24 == NRF_CTRLAP && ap_reg(s, a) == NRF_CTRLAP_RESET) {
		s->ctrlap_reset = v;
		if (s->reset_writes < 8) {
			s->reset_log[s->reset_writes] = v;
		}
		s->reset_writes++;
	}
}

static void lockout(struct swd_sim *s)
{
	s->phase = PH_LOCKOUT;
	s->lockouts++;
}

/* the request is in: decide the answer */
static void request(struct swd_sim *s)
{
	uint8_t r = s->req;
	bool ap = r >> 1 & 1, rd = r >> 2 & 1;
	uint8_t a = (uint8_t)((r >> 3 & 3) << 2);
	int par = (r >> 1 & 1) ^ (r >> 2 & 1) ^ (r >> 3 & 1) ^ (r >> 4 & 1);

	if ((r & 1) != 1 || (r >> 5 & 1) != par || (r >> 6 & 1) != 0 || (r >> 7 & 1) != 1) {
		lockout(s);
		return;
	}
	if (s->need_idr && (ap || !rd || a != SWD_DP_DPIDR)) {
		lockout(s);
		return;
	}
	s->phase = PH_RESP;
	s->pos = 0;
	if (ap && s->wait_n) {
		s->wait_n--;
		s->ack = SWD_ACK_WAIT;
		return;
	}
	bool abort = !ap && !rd && a == SWD_DP_ABORT;

	if (s->sticky && !abort && !(!ap && rd)) {
		s->ack = SWD_ACK_FAULT;
		return;
	}
	s->ack = SWD_ACK_OK;
	if (!rd) {
		return;
	}
	if (ap) {
		s->out = s->rdbuff; /* posted: the last AP read */
		s->rdbuff = ap_read(s, a);
		return;
	}
	switch (a) {
	case SWD_DP_DPIDR:
		s->out = s->dpidr;
		s->need_idr = false;
		break;
	case SWD_DP_CTRLSTAT: {
		uint32_t v = s->ctrlstat;

		if (s->pwr_acks) {
			v |= (v & 0x40000000u) << 1 | (v & 0x10000000u) << 1;
		}
		s->out = v;
		break;
	}
	case SWD_DP_RDBUFF:
		s->out = s->rdbuff;
		break;
	default:
		s->out = 0;
	}
}

static void write_done(struct swd_sim *s, int parity)
{
	bool ap = s->req >> 1 & 1;
	uint8_t a = (uint8_t)((s->req >> 3 & 3) << 2);

	if (parity != swd_parity32(s->in)) {
		s->sticky = true; /* WDATAERR */
		return;
	}
	if (ap) {
		ap_write(s, a, s->in);
		return;
	}
	s->dp_writes++;
	switch (a) {
	case SWD_DP_ABORT:
		if (s->in & SWD_ABORT_CLEAR) {
			s->sticky = false;
		}
		break;
	case SWD_DP_CTRLSTAT:
		s->ctrlstat = s->in & 0x50000000u;
		s->last_ctrlstat_write = s->in;
		break;
	case SWD_DP_SELECT:
		s->select = s->in;
		break;
	default:
		break;
	}
}

/* drive bit `b` until the next rising edge */
static void drive(struct swd_sim *s, int b)
{
	s->tgt_drives = true;
	s->tgt_level = b;
}

static void respond(struct swd_sim *s, int bit)
{
	unsigned p = ++s->pos; /* 1 is the turnaround edge after the request */
	bool rd = s->req >> 2 & 1;

	if (p <= 3) {
		drive(s, s->ack >> (p - 1) & 1);
		return;
	}
	if (s->ack == SWD_ACK_OK && rd) {
		if (p <= 35) {
			drive(s, (int)(s->out >> (p - 4)) & 1);
		} else if (p == 36) {
			drive(s, swd_parity32(s->out) ^ (s->bad_rparity ? 1 : 0));
		} else if (p == 37) {
			s->tgt_drives = false;
		} else {
			s->phase = PH_IDLE; /* p 38: the host's turnaround */
		}
		return;
	}
	s->tgt_drives = false;
	if (s->ack == SWD_ACK_OK) {
		/* p 4 released, p 5 turnaround, p 6..37 data, p 38 parity */
		if (p >= 6 && p <= 37) {
			if (p == 6) {
				s->in = 0;
			}
			s->in |= (uint32_t)bit << (p - 6);
		} else if (p == 38) {
			write_done(s, bit);
			s->phase = PH_IDLE;
		}
		return;
	}
	if (p >= 5) {
		s->phase = PH_IDLE; /* WAIT or FAULT: no data phase */
	}
}

static void edge(struct swd_sim *s)
{
	int bit = line(s);

	s->edges++;
	if (s->host_drives && s->tgt_drives) {
		s->contention++;
	}
	if (!s->present) {
		return;
	}
	/* line reset and the JTAG-to-SWD switch, seen in any phase while the host drives */
	if (s->host_drives) {
		s->ones = bit ? s->ones + 1 : 0;
		s->shift = (uint16_t)(s->shift >> 1 | (unsigned)bit << 15);
		if (s->ones == 50) {
			s->reset_seen = true;
			s->tgt_drives = false;
			if (s->selected) {
				s->phase = PH_WAIT_IDLE;
				s->need_idr = true;
			}
			return;
		}
		if (!s->selected && s->reset_seen && s->shift == 0xE79E) {
			s->selected = true;
			s->phase = PH_LOCKOUT; /* a line reset must follow */
			return;
		}
	}
	switch (s->phase) {
	case PH_WAIT_IDLE:
		if (!bit) {
			s->phase = PH_IDLE;
		}
		break;
	case PH_IDLE:
		if (bit && s->host_drives) {
			s->req = 1;
			s->nbits = 1;
			s->phase = PH_REQ;
		}
		break;
	case PH_REQ:
		s->req |= (uint8_t)(bit << s->nbits);
		if (++s->nbits == 8) {
			request(s);
		}
		break;
	case PH_RESP:
		respond(s, bit);
		break;
	default:
		break; /* off or locked out until a line reset */
	}
}

static void io_clk(void *ctx, int level)
{
	struct swd_sim *s = ctx;

	if (!s->clk && level) {
		edge(s);
	}
	s->clk = level;
}

static void io_dio(void *ctx, int level)
{
	((struct swd_sim *)ctx)->host_level = level;
}

static int io_dio_in(void *ctx)
{
	return line(ctx);
}

static void io_dir(void *ctx, int host)
{
	((struct swd_sim *)ctx)->host_drives = host;
}

static void io_delay(void *ctx)
{
	(void)ctx;
}

struct swd_io swd_sim_io(struct swd_sim *s)
{
	return (struct swd_io){io_clk, io_dio, io_dio_in, io_dir, io_delay, s};
}
