/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The SWD host against the bit-level target (swd_sim.c).
 */
#include "check.h"
#include "swd.h"
#include "swd_sim.h"

static struct swd_sim S;
static struct swd_io io;

static void sim(void)
{
	swd_sim_init(&S);
	io = swd_sim_io(&S);
}

static void request_bytes(void)
{
	/* the well-known request bytes */
	CHECK_EQ(swd_request(false, true, SWD_DP_DPIDR), 0xA5);
	CHECK_EQ(swd_request(false, false, SWD_DP_ABORT), 0x81);
	CHECK_EQ(swd_request(false, false, SWD_DP_CTRLSTAT), 0xA9);
	CHECK_EQ(swd_request(false, true, SWD_DP_CTRLSTAT), 0x8D);
	CHECK_EQ(swd_request(false, false, SWD_DP_SELECT), 0xB1);
	CHECK_EQ(swd_request(false, true, SWD_DP_RDBUFF), 0xBD);
	CHECK_EQ(swd_request(true, true, 0xC), 0x9F);
	CHECK_EQ(swd_request(true, false, 0x0), 0xA3);
	CHECK_EQ(swd_parity32(0), 0);
	CHECK_EQ(swd_parity32(1), 1);
	CHECK_EQ(swd_parity32(0x80000001u), 0);
	CHECK_EQ(swd_parity32(0x02880000u), 1);
	CHECK_EQ(swd_parity32(0xFFFFFFFFu), 0);
}

static void connect_and_idr(void)
{
	uint32_t dpidr = 0, idr = 0;

	sim();
	CHECK_EQ(swd_connect(&io, &dpidr), SWD_OK);
	CHECK_EQ(dpidr, 0x2BA01477u);
	CHECK(S.selected);
	CHECK_EQ(S.ctrlstat, SWD_PWRUP_REQ);
	CHECK_EQ(swd_ap_read(&io, NRF_CTRLAP, NRF_CTRLAP_IDR, &idr), SWD_OK);
	CHECK_EQ(idr, NRF52_CTRLAP_IDR);
	CHECK_EQ(S.contention, 0);
	CHECK_EQ(S.lockouts, 0);
}

static void hard_reset_sequence(void)
{
	uint32_t idr = 0;

	sim();
	CHECK_EQ(swd_nrf_hard_reset(&io, 1000, &idr), SWD_OK);
	CHECK_EQ(idr, NRF52_CTRLAP_IDR);
	CHECK_EQ(S.reset_writes, 2);
	CHECK_EQ(S.reset_log[0], 1);
	CHECK_EQ(S.reset_log[1], 0);
	CHECK_EQ(S.ctrlap_reset, 0);
	CHECK_EQ(S.last_ctrlstat_write, 0); /* debug power released */
	CHECK(!S.host_drives);              /* SWDIO left undriven */
	CHECK_EQ(S.contention, 0);
	CHECK_EQ(S.lockouts, 0);
}

static void no_pod(void)
{
	uint32_t idr = 0, dpidr;

	/* nothing on the pins: the case's 1M pull-down reads 0 */
	sim();
	S.present = false;
	S.pull = 0;
	CHECK_EQ(swd_connect(&io, &dpidr), SWD_E_PROTO);
	CHECK_EQ(swd_nrf_hard_reset(&io, 10, &idr), SWD_E_PROTO);
	CHECK(!S.host_drives);
	/* an undriven line with a pull-up reads ACK 0b111 */
	sim();
	S.present = false;
	S.pull = 1;
	CHECK_EQ(swd_connect(&io, &dpidr), SWD_E_PROTO);
}

static void wait_is_retried(void)
{
	uint32_t v;

	sim();
	CHECK_EQ(swd_connect(&io, &v), SWD_OK);
	S.wait_n = 5;
	CHECK_EQ(swd_ap_read(&io, NRF_CTRLAP, NRF_CTRLAP_IDR, &v), SWD_OK);
	CHECK_EQ(v, NRF52_CTRLAP_IDR);
	S.wait_n = 1000;
	CHECK_EQ(swd_ap_read(&io, NRF_CTRLAP, NRF_CTRLAP_IDR, &v), SWD_E_WAIT);
	CHECK_EQ(S.contention, 0);
}

static void read_parity_error(void)
{
	uint32_t v;

	sim();
	S.bad_rparity = true;
	CHECK_EQ(swd_connect(&io, &v), SWD_E_PARITY);
}

static void not_an_nrf52(void)
{
	uint32_t idr;

	sim();
	S.idr = 0x12345678;
	CHECK_EQ(swd_nrf_hard_reset(&io, 10, &idr), SWD_E_ID);
	CHECK_EQ(S.reset_writes, 0);
}

static void sticky_error_cleared(void)
{
	uint32_t v;

	sim();
	S.sticky = true;
	CHECK_EQ(swd_connect(&io, &v), SWD_OK);
	CHECK(!S.sticky);
}

static void target_needs_selection(void)
{
	uint32_t v;

	/* a transfer without the line reset and JTAG-to-SWD gets no answer */
	sim();
	CHECK_EQ(swd_xfer(&io, false, true, SWD_DP_DPIDR, &v), SWD_E_PROTO);
	/* after a line reset, anything but DPIDR locks the target out */
	sim();
	swd_line_reset(&io);
	CHECK_EQ(swd_xfer(&io, false, true, SWD_DP_CTRLSTAT, &v), SWD_E_PROTO);
	CHECK_EQ(S.lockouts, 1);
	CHECK_EQ(swd_connect(&io, &v), SWD_OK); /* a new line reset recovers */
}

static void reset_twice(void)
{
	uint32_t idr;

	sim();
	CHECK_EQ(swd_nrf_hard_reset(&io, 10, &idr), SWD_OK);
	CHECK_EQ(swd_nrf_hard_reset(&io, 10, &idr), SWD_OK);
	CHECK_EQ(S.reset_writes, 4);
	CHECK_EQ(S.contention, 0);
}

const struct check_case swd_cases[] = {
	{"swd: request bytes and parity", request_bytes},
	{"swd: connect and CTRL-AP IDR", connect_and_idr},
	{"swd: hard reset sequence", hard_reset_sequence},
	{"swd: no pod on the pins", no_pod},
	{"swd: WAIT retried", wait_is_retried},
	{"swd: read parity error", read_parity_error},
	{"swd: not an nRF52", not_an_nrf52},
	{"swd: sticky error cleared", sticky_error_cleared},
	{"swd: target needs selection", target_needs_selection},
	{"swd: reset twice", reset_twice},
	CASES_END,
};
