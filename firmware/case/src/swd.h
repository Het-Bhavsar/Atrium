/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * A bit-banged SWD host (ARM ADIv5 SW-DP), only as far as the nRF52832 CTRL-AP: line
 * reset with the JTAG-to-SWD switch, DP reads and writes, AP reads and writes, and the hard
 * reset through CTRL-AP RESET, which works with a hung application and with APPROTECT on
 * (nRF52832 PS, CTRL-AP). Plain C over pin callbacks, so the host tests drive it against a
 * bit-level target model (tests/swd_sim.c).
 * Bit timing (as CMSIS-DAP): the host changes SWDIO while SWCLK is low; the target samples
 * on the rising edge, and drives its bits after a rising edge for the host to read in the
 * next low phase. Each turnaround is one clock with nobody driving.
 */
#ifndef CASE_SWD_H_
#define CASE_SWD_H_

#include <stdbool.h>
#include <stdint.h>

struct swd_io {
	void (*clk)(void *ctx, int level);
	void (*dio)(void *ctx, int level);
	int (*dio_in)(void *ctx);
	void (*dio_dir)(void *ctx, int host_drives); /* the 74LVC1T45 DIR */
	void (*delay)(void *ctx);                    /* half a clock period */
	void *ctx;
};

/* ACK as read, LSB first */
enum { SWD_ACK_OK = 1, SWD_ACK_WAIT = 2, SWD_ACK_FAULT = 4 };

enum swd_err {
	SWD_OK = 0,
	SWD_E_WAIT = -1,   /* WAIT on every retry */
	SWD_E_FAULT = -2,  /* FAULT: a sticky error */
	SWD_E_PROTO = -3,  /* no valid ACK: no target, or it lost sync */
	SWD_E_PARITY = -4, /* read data parity */
	SWD_E_ID = -5,     /* CTRL-AP IDR is not an nRF52 one */
};

/* DP registers (A[3:2]) */
#define SWD_DP_DPIDR    0x0 /* read */
#define SWD_DP_ABORT    0x0 /* write */
#define SWD_DP_CTRLSTAT 0x4
#define SWD_DP_SELECT   0x8
#define SWD_DP_RDBUFF   0xC

#define SWD_ABORT_CLEAR    0x1Eu       /* STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR */
#define SWD_PWRUP_REQ      0x50000000u /* CSYSPWRUPREQ | CDBGPWRUPREQ */
#define SWD_PWRUP_ACK      0xA0000000u /* CSYSPWRUPACK | CDBGPWRUPACK */

/* nRF52832 CTRL-AP, AP #1 (nRF52832 PS, "CTRL-AP") */
#define NRF_CTRLAP          1
#define NRF_CTRLAP_RESET    0x000
#define NRF_CTRLAP_ERASEALL 0x004
#define NRF_CTRLAP_ERASEALLSTATUS 0x008
#define NRF_CTRLAP_APPROTECTSTATUS 0x00C
#define NRF_CTRLAP_IDR      0x0FC
#define NRF52_CTRLAP_IDR    0x02880000u /* [VERIFY on a pod] */

#define SWD_WAIT_RETRIES 100

/* The 8-bit request: start, APnDP, RnW, A[2:3], parity, stop, park. */
uint8_t swd_request(bool ap, bool read, uint8_t addr);

/* Even parity: 1 when v has an odd number of ones. */
int swd_parity32(uint32_t v);

/* Line reset, JTAG-to-SWD (0xE79E), line reset, idle. */
void swd_line_reset(const struct swd_io *io);

/* `n` idle clocks with SWDIO low. */
void swd_idle(const struct swd_io *io, unsigned n);

/* One transfer, retried on WAIT. Reads fill *data. */
int swd_xfer(const struct swd_io *io, bool ap, bool read, uint8_t addr, uint32_t *data);

/* Line reset, DPIDR, clear sticky errors, debug power-up (the ACKs are not required). */
int swd_connect(const struct swd_io *io, uint32_t *dpidr);

int swd_ap_read(const struct swd_io *io, uint8_t apsel, uint16_t reg, uint32_t *v);
int swd_ap_write(const struct swd_io *io, uint8_t apsel, uint16_t reg, uint32_t v);

/*
 * The pod hard reset: connect, check CTRL-AP IDR, RESET = 1, hold `hold_clocks` idle
 * clocks, RESET = 0, then drop the debug power-up request so the nRF can leave debug
 * interface mode [VERIFY current after the reset]. SWDIO is left undriven.
 */
int swd_nrf_hard_reset(const struct swd_io *io, unsigned hold_clocks, uint32_t *idr);

#endif /* CASE_SWD_H_ */
