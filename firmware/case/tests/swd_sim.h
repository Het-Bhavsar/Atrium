/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * A bit-level SW-DP target with an nRF52832 CTRL-AP, for the host tests. It sees only the
 * pins, edge by edge, as a pod would through the translators:
 *  - selection needs a line reset, 0xE79E, another line reset; after each line reset only
 *    a DPIDR read is answered (anything else locks it out until the next line reset);
 *  - a request with a bad parity, stop or park bit locks it out the same way;
 *  - it samples on the rising edge and drives after it; both driving at an edge is counted
 *    as contention;
 *  - AP reads are posted (RDBUFF), WAIT can be injected, and a host data parity error
 *    makes it answer FAULT until ABORT.
 */
#ifndef SWD_SIM_H_
#define SWD_SIM_H_

#include <stdbool.h>
#include <stdint.h>

#include "swd.h"

struct swd_sim {
	/* pins */
	int clk;
	int host_level;
	bool host_drives;
	int tgt_level;
	bool tgt_drives;
	int pull;            /* the undriven level: 1 nRF pull-up, 0 no pod (case 1M pull-down) */
	bool present;        /* false: no pod, never drives */
	/* link */
	unsigned ones;
	uint16_t shift;
	bool reset_seen;
	bool selected;       /* JTAG-to-SWD seen */
	int phase;
	bool need_idr;       /* after a line reset, only DPIDR */
	uint8_t req;
	unsigned nbits;
	unsigned pos;
	int ack;
	uint32_t out;        /* read data */
	uint32_t in;         /* write data */
	/* registers */
	uint32_t dpidr;
	uint32_t ctrlstat;
	uint32_t select;
	uint32_t rdbuff;
	bool sticky;
	bool pwr_acks;       /* the power-up ACK bits follow the requests */
	uint32_t ctrlap_reset;
	uint32_t idr;
	/* injected faults */
	unsigned wait_n;     /* the next n AP accesses answer WAIT */
	bool bad_rparity;    /* corrupt the parity of read data */
	/* log */
	unsigned contention;
	unsigned edges;
	unsigned lockouts;
	unsigned reset_writes;
	uint32_t reset_log[8];
	unsigned dp_writes;
	uint32_t last_ctrlstat_write;
};

void swd_sim_init(struct swd_sim *s);
/* swd_io callbacks for the host under test, with ctx = the sim */
struct swd_io swd_sim_io(struct swd_sim *s);

#endif /* SWD_SIM_H_ */
