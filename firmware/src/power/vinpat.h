/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * VIN drop-out patterns from the charging case, seen on the BQ21061 /PG pin
 * (docs/charging-case.md; the case side is firmware/case/src/case_sm.c).
 *  - A drop of 100 to 400 ms is a pulse. Shorter is contact bounce and is ignored; longer
 *    is a lift or the case switching VIN off, and clears the count.
 *  - Two pulses within 2 s: the case signature (decision 2026-11-08). The pod is
 *    fed by the case (not the Plinth) until VIN next goes away for longer than a pulse.
 *  - Three pulses within 2 s: the soft reset. The pod reboots.
 * Plain C, fed from the /PG interrupt with the uptime in ms.
 */
#ifndef ATRIUM_VINPAT_H_
#define ATRIUM_VINPAT_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vinpat_cfg {
	uint16_t min_off_ms;
	uint16_t max_off_ms;
	uint16_t window_ms; /* from the first pulse's start to the last pulse's end */
};

#define VINPAT_CFG_DEFAULT {.min_off_ms = 100, .max_off_ms = 400, .window_ms = 2000}

enum vinpat_ev {
	VINPAT_NONE = 0,
	VINPAT_CASE,  /* the second pulse: fed by the case */
	VINPAT_RESET, /* the third pulse: reboot */
};

struct vinpat {
	struct vinpat_cfg cfg;
	bool vin;
	bool off;
	uint32_t off_t0;
	uint8_t n;
	uint32_t start[3];
	bool on_case;
};

void vinpat_init(struct vinpat *v, const struct vinpat_cfg *cfg, bool vin);

/* A /PG reading (vin = /PG low) at an edge. Same-level calls are ignored. */
enum vinpat_ev vinpat_edge(struct vinpat *v, bool vin, uint32_t now_ms);

/* Fed by the case now: VIN present and the signature seen since it came on for good. */
bool vinpat_on_case(const struct vinpat *v);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_VINPAT_H_ */
