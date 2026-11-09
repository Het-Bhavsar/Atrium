/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Vibration capture, research modes 10 and 11 (docs/ble_gatt.md 4.4): the LSM6DSOX
 * accelerometer alone in high-performance mode at 3333 or 6667 Hz, +-2 g, LPF2 off, batched in
 * its FIFO and read in bursts; one axis goes out on Vibration Data (0x0207). Register writes,
 * FIFO parsing and the packet layout are here, in plain C, so the host tests pin them; the I2C
 * and the drain thread are in hw/sensors.c. Register facts: DS12814 Rev 3, sections 6.4.1, 6.6
 * and 9.
 */
#ifndef ATRIUM_VIBCAP_H_
#define ATRIUM_VIBCAP_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "modes.h"

/* LSM6DSOX main-page registers */
#define VIB_REG_FIFO_CTRL1   0x07 /* WTM[7:0] */
#define VIB_REG_FIFO_CTRL2   0x08 /* WTM8 and compression: all 0 */
#define VIB_REG_FIFO_CTRL3   0x09 /* BDR_GY[7:4], BDR_XL[3:0] */
#define VIB_REG_FIFO_CTRL4   0x0A /* FIFO_MODE[2:0]: 000 bypass, 001 FIFO (stop when full) */
#define VIB_REG_CTRL1_XL     0x10 /* ODR_XL[7:4], FS_XL[3:2], LPF2_XL_EN[1] */
#define VIB_REG_CTRL6_C      0x15 /* XL_HM_MODE[4]: 0 high-performance, 1 low-power/normal */
#define VIB_REG_FIFO_STATUS1 0x3A /* DIFF_FIFO[7:0], then FIFO_STATUS2 */
#define VIB_REG_FIFO_OUT     0x78 /* FIFO_DATA_OUT_TAG, then X, Y, Z little-endian */

#define VIB_CTRL6_LP_NORMAL 0x10 /* accel-pm = LSM6DSO_DT_XL_LP_NORMAL_MODE in atrium_pod.dts */
#define VIB_FIFO_FIFO_MODE  0x01

/* One FIFO word is a tag byte and 6 data bytes; a burst is 36 words, 252 B, because the
 * nRF52832 TWIM moves at most 255 B per transfer (8-bit MAXCNT). The watermark is one burst. */
#define VIB_WORD        7
#define VIB_BURST_WORDS 36
#define VIB_WTM         VIB_BURST_WORDS
#define VIB_TAG_XL_NC   0x02 /* accelerometer, not compressed (Table 196) */

/* Vibration Data notification: 14-byte header, then up to 115 samples (a 247-byte ATT MTU) */
#define VIB_HDR_LEN     14
#define VIB_MAX_SAMPLES 115
#define VIB_MAX_PKT     (VIB_HDR_LEN + 2 * VIB_MAX_SAMPLES)

#define VIB_FLAG_GAP     0x01 /* the FIFO filled and restarted: samples lost before this one */
#define VIB_FLAG_SKIPPED 0x02 /* a FIFO word with another tag was skipped (not a sample) */

struct vib_reg {
	uint8_t reg;
	uint8_t val;
};

#define VIB_MAX_WRITES 12

/* CTRL1_XL ODR_XL and FIFO_CTRL3 BDR_XL code: 9 (3333 Hz) or 10 (6667 Hz); 0 for other modes */
uint8_t vib_odr_code(enum pod_mode m);

/* Nominal rate for the header, Hz: 3333 or 6667; 0 for other modes */
uint16_t vib_rate_hz(enum pod_mode m);

/*
 * The writes that take the accelerometer from mode `from` to mode `to`. Leaving a vibration
 * mode powers the accelerometer down, empties the FIFO and puts it back in low-power/normal
 * mode, so the Zephyr driver's ODR call for the next mode starts from there; entering one
 * powers down, sets high-performance, the watermark and the batch rate, starts FIFO mode and
 * then sets the ODR, +-2 g and LPF2 off. Other pairs need no writes. Returns the count.
 */
size_t vib_transition(enum pod_mode from, enum pod_mode to, struct vib_reg out[VIB_MAX_WRITES]);

/* After the FIFO filled: bypass (empties it), then FIFO mode again. */
size_t vib_restart(struct vib_reg out[2]);

/* FIFO_STATUS1 and FIFO_STATUS2 as read together */
struct vib_status {
	uint16_t level; /* unread words */
	bool wtm;       /* level >= watermark */
	bool stopped;   /* the FIFO filled (FIFO mode stops there): later samples are lost */
};
struct vib_status vib_parse_status(const uint8_t st[2]);

struct vib_hdr {
	uint16_t seq;     /* +1 per notification */
	uint16_t rate_hz; /* nominal ODR */
	uint32_t t_ms;    /* uptime ms of the first sample [ASSUMED within one sample] */
	uint32_t index;   /* FIFO sample count of the first sample since the capture started */
	uint8_t mode;     /* 10 or 11 */
	uint8_t flags;    /* VIB_FLAG_* */
};

size_t vib_encode(const struct vib_hdr *h, const int16_t *s, size_t n, uint8_t *out);
/* Returns the number of samples, or -1 for a malformed packet. */
int vib_decode(const uint8_t *p, size_t len, struct vib_hdr *h, int16_t *s, size_t max);

typedef void (*vib_emit_t)(const uint8_t *pkt, size_t len, void *user);

struct vib_cap {
	uint8_t mode;
	uint8_t axis; /* chip axis, 0 x, 1 y, 2 z */
	bool neg;     /* pod axis = -chip axis */
	uint8_t max;  /* samples per notification */
	uint16_t hz;
	uint32_t imu_mhz;
	uint16_t seq;
	uint32_t index; /* index of the next sample from the FIFO */
	uint8_t flags;  /* for the packet being filled */
	uint8_t n;
	uint32_t buf_index;
	uint64_t buf_t_us;
	int16_t buf[VIB_MAX_SAMPLES];
	/* vib_cap_clock() */
	bool clk_stopped;
	uint16_t clk_left;    /* words the last running status read left unread */
	uint64_t clk_last_us; /* time of that read */
	uint64_t clk_stop_us; /* when the newest word of the stopped FIFO was made */
};

/* Start a capture: sample index 0, sequence 0. `room` is the notification payload (MTU - 3). */
void vib_cap_start(struct vib_cap *c, enum pod_mode m, uint8_t axis, bool neg, size_t room);

/*
 * One burst: `n` FIFO words read at `t_us`, the oldest `n` of the `level` words that the
 * status read just before it reported. Full packets go to `emit`.
 */
void vib_cap_burst(struct vib_cap *c, const uint8_t *raw, size_t n, uint16_t level,
		   uint64_t t_us, vib_emit_t emit, void *user);

/*
 * The `t_us` to give vib_cap_burst() after a status read at `t_us`. While the FIFO runs that
 * is `t_us` itself. FIFO mode stops when the FIFO is full, so its newest word was made then,
 * not at the read: from the first stopped status until the restart this returns that moment,
 * worked out from the last running status and the words since, and the words drained from
 * the stopped FIFO keep their true times. `stopped` is the caller's flag (set on a stopped
 * status, cleared at the restart); `n_read` the words the caller reads after this status.
 */
uint64_t vib_cap_clock(struct vib_cap *c, const struct vib_status *fs, bool stopped,
		       size_t n_read, uint64_t t_us);

/* The FIFO filled and was restarted: send what is held, and flag the next sample. */
void vib_cap_gap(struct vib_cap *c, vib_emit_t emit, void *user);

#endif /* ATRIUM_VIBCAP_H_ */
