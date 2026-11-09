/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "swd.h"

int swd_parity32(uint32_t v)
{
	v ^= v >> 16;
	v ^= v >> 8;
	v ^= v >> 4;
	v ^= v >> 2;
	v ^= v >> 1;
	return (int)(v & 1);
}

uint8_t swd_request(bool ap, bool read, uint8_t addr)
{
	uint8_t a = (addr >> 2) & 3;
	uint8_t body = (uint8_t)((ap ? 1 : 0) | (read ? 2 : 0) | (a << 2)); /* APnDP RnW A2 A3 */

	return (uint8_t)(0x81 | body << 1 | swd_parity32(body) << 5);
}

static void wbit(const struct swd_io *io, int b)
{
	io->dio(io->ctx, b);
	io->clk(io->ctx, 0);
	io->delay(io->ctx);
	io->clk(io->ctx, 1);
	io->delay(io->ctx);
}

static int rbit(const struct swd_io *io)
{
	io->clk(io->ctx, 0);
	io->delay(io->ctx);
	int b = io->dio_in(io->ctx) ? 1 : 0;

	io->clk(io->ctx, 1);
	io->delay(io->ctx);
	return b;
}

static void turn(const struct swd_io *io)
{
	io->clk(io->ctx, 0);
	io->delay(io->ctx);
	io->clk(io->ctx, 1);
	io->delay(io->ctx);
}

static void wbits(const struct swd_io *io, uint32_t v, unsigned n)
{
	for (unsigned i = 0; i < n; i++) {
		wbit(io, (int)(v >> i) & 1);
	}
}

void swd_idle(const struct swd_io *io, unsigned n)
{
	io->dio_dir(io->ctx, 1);
	for (unsigned i = 0; i < n; i++) {
		wbit(io, 0);
	}
}

static void ones(const struct swd_io *io, unsigned n)
{
	for (unsigned i = 0; i < n; i++) {
		wbit(io, 1);
	}
}

void swd_line_reset(const struct swd_io *io)
{
	io->dio_dir(io->ctx, 1);
	ones(io, 56);
	wbits(io, 0xE79E, 16); /* JTAG-to-SWD, LSB first */
	ones(io, 56);
	swd_idle(io, 8);
}

static int xfer_once(const struct swd_io *io, uint8_t req, bool read, uint32_t *data)
{
	io->dio_dir(io->ctx, 1);
	wbits(io, req, 8);
	io->dio_dir(io->ctx, 0);
	turn(io);

	int ack = 0;

	for (int i = 0; i < 3; i++) {
		ack |= rbit(io) << i;
	}
	if (ack == SWD_ACK_OK && read) {
		uint32_t v = 0;

		for (int i = 0; i < 32; i++) {
			v |= (uint32_t)rbit(io) << i;
		}
		int p = rbit(io);

		turn(io);
		io->dio_dir(io->ctx, 1);
		swd_idle(io, 8);
		*data = v;
		return p == swd_parity32(v) ? SWD_OK : SWD_E_PARITY;
	}
	turn(io);
	io->dio_dir(io->ctx, 1);
	if (ack == SWD_ACK_OK) {
		wbits(io, *data, 32);
		wbit(io, swd_parity32(*data));
		swd_idle(io, 8); /* clocks for the posted write to complete */
		return SWD_OK;
	}
	swd_idle(io, 8);
	return ack == SWD_ACK_WAIT ? SWD_E_WAIT : ack == SWD_ACK_FAULT ? SWD_E_FAULT : SWD_E_PROTO;
}

int swd_xfer(const struct swd_io *io, bool ap, bool read, uint8_t addr, uint32_t *data)
{
	uint8_t req = swd_request(ap, read, addr);
	int ret = SWD_E_WAIT;

	for (int i = 0; i < SWD_WAIT_RETRIES && ret == SWD_E_WAIT; i++) {
		ret = xfer_once(io, req, read, data);
	}
	return ret;
}

static int dp_read(const struct swd_io *io, uint8_t addr, uint32_t *v)
{
	return swd_xfer(io, false, true, addr, v);
}

static int dp_write(const struct swd_io *io, uint8_t addr, uint32_t v)
{
	return swd_xfer(io, false, false, addr, &v);
}

int swd_connect(const struct swd_io *io, uint32_t *dpidr)
{
	int ret = SWD_E_PROTO;

	for (int i = 0; i < 2 && ret == SWD_E_PROTO; i++) {
		swd_line_reset(io);
		ret = dp_read(io, SWD_DP_DPIDR, dpidr);
	}
	if (ret) {
		return ret;
	}
	ret = dp_write(io, SWD_DP_ABORT, SWD_ABORT_CLEAR);
	ret = ret ? ret : dp_write(io, SWD_DP_CTRLSTAT, SWD_PWRUP_REQ);
	for (int i = 0; i < 100 && !ret; i++) {
		uint32_t st;

		ret = dp_read(io, SWD_DP_CTRLSTAT, &st);
		if (!ret && (st & SWD_PWRUP_ACK) == SWD_PWRUP_ACK) {
			break;
		}
	}
	return ret;
}

static int select_ap(const struct swd_io *io, uint8_t apsel, uint16_t reg)
{
	return dp_write(io, SWD_DP_SELECT, (uint32_t)apsel << 24 | (reg & 0xF0));
}

int swd_ap_read(const struct swd_io *io, uint8_t apsel, uint16_t reg, uint32_t *v)
{
	uint32_t posted;
	int ret = select_ap(io, apsel, reg);

	/* AP reads are posted: the value comes from RDBUFF */
	ret = ret ? ret : swd_xfer(io, true, true, (uint8_t)(reg & 0xC), &posted);
	return ret ? ret : dp_read(io, SWD_DP_RDBUFF, v);
}

int swd_ap_write(const struct swd_io *io, uint8_t apsel, uint16_t reg, uint32_t v)
{
	int ret = select_ap(io, apsel, reg);

	return ret ? ret : swd_xfer(io, true, false, (uint8_t)(reg & 0xC), &v);
}

int swd_nrf_hard_reset(const struct swd_io *io, unsigned hold_clocks, uint32_t *idr)
{
	uint32_t dpidr;
	int ret = swd_connect(io, &dpidr);

	ret = ret ? ret : swd_ap_read(io, NRF_CTRLAP, NRF_CTRLAP_IDR, idr);
	if (!ret && *idr != NRF52_CTRLAP_IDR) {
		ret = SWD_E_ID;
	}
	if (!ret) {
		ret = swd_ap_write(io, NRF_CTRLAP, NRF_CTRLAP_RESET, 1);
		swd_idle(io, hold_clocks);
		ret = ret ? ret : swd_ap_write(io, NRF_CTRLAP, NRF_CTRLAP_RESET, 0);
		(void)dp_write(io, SWD_DP_CTRLSTAT, 0); /* release the debug power-up */
	}
	io->dio_dir(io->ctx, 0);
	return ret;
}
