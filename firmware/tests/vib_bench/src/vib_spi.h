/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Vibration bench rig core: the LSM6DSOX on SPI, set up as mode 11 (vibcap.h: 6667 Hz,
 * high-performance, +-2 g, LPF2 off, accelerometer-only FIFO), and one drain step that turns
 * FIFO bursts into Vibration Data packets with vibcap.c unchanged. Plain C over a transfer
 * callback, so the host tests run it against a simulated chip with SPI timing. The Zephyr
 * main (main.c) supplies SPI, the clock and the serial port. docs/ble_gatt.md 4.5.
 */
#ifndef ATRIUM_VIB_SPI_H_
#define ATRIUM_VIB_SPI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vibcap.h"

#define VIB_SPI_READ        0x80 /* first byte: bit 7 set to read, 6:0 the register */
#define VIB_SPI_WHO_AM_I    0x0F
#define VIB_SPI_WHO_AM_I_ID 0x6C
#define VIB_SPI_CTRL3_C     0x12 /* BOOT[7] BDU[6] ... IF_INC[2] SW_RESET[0] */
#define VIB_SPI_CTRL4_C     0x13 /* I2C_disable[2] */
#define VIB_SPI_CTRL3_RESET 0x01
#define VIB_SPI_CTRL3_RUN   0x04 /* IF_INC: the burst read walks the FIFO output registers */
#define VIB_SPI_CTRL4_RUN   0x04 /* I2C off: SPI only, so SDA/SCL activity cannot confuse it */

/* the pod's axis rule (atrium_pod.dts imu-to-pod, pod z row 0 0 1): chip +Z, not negated */
#define VIB_SPI_AXIS 2
#define VIB_SPI_NEG  false

/* Burst: the address byte and 36 words, 253 B, under the nRF52832 SPIM's 255-byte MAXCNT.
 * Every transfer is 2 bytes or more (nRF52832 anomaly 58 needs RXD.MAXCNT 1 to clock a byte
 * extra). */
#define VIB_SPI_BUF (1 + VIB_BURST_WORDS * VIB_WORD)

/* "0207 ", two hex digits per byte, "\n" */
#define VIB_LINE_MAX (5 + 2 * VIB_MAX_PKT + 1)

/* Full-duplex transfer of `len` bytes under one chip-select; 0 or a negative error. */
typedef int (*vib_spi_xfer_t)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
typedef uint64_t (*vib_spi_now_t)(void *ctx);

struct vib_spi_stats {
	uint32_t polls;     /* status reads */
	uint32_t bursts;    /* FIFO reads */
	uint32_t words;     /* FIFO words read */
	uint32_t gaps;      /* times the FIFO filled and restarted */
	uint32_t errors;    /* failed transfers */
	uint16_t max_level; /* most unread words seen at a status read */
};

struct vib_spi {
	vib_spi_xfer_t xfer;
	vib_spi_now_t now_us;
	void *ctx;
	struct vib_cap cap;
	struct vib_spi_stats st;
	bool stopped;
	uint8_t tx[VIB_SPI_BUF];
	uint8_t rx[VIB_SPI_BUF];
};

int vib_spi_write(struct vib_spi *s, uint8_t reg, uint8_t val);
int vib_spi_read(struct vib_spi *s, uint8_t reg, uint8_t *out, size_t n);

/* Before the setup: 0 when WHO_AM_I reads 0x6C. */
int vib_spi_probe(struct vib_spi *s);

/*
 * Mode 11 from a reset chip: I2C off, auto-increment on, then vib_transition()'s entry writes.
 * Starts the capture: sample index 0, sequence 0, 115 samples a packet.
 */
int vib_spi_start(struct vib_spi *s);

/*
 * One drain step, as the pod's (hw/sensors.c): read FIFO_STATUS1/2; with the watermark up, or
 * once the FIFO has stopped full, read up to 36 words into vib_cap_burst(); after emptying a
 * stopped FIFO, restart it and flag the gap. Returns the words read (0: nothing to do yet) or
 * a negative error. Full packets go to `emit`.
 */
int vib_spi_poll(struct vib_spi *s, vib_emit_t emit, void *user);

/* "0207 <lowercase hex>\n" for one packet; returns the length (no NUL is written). */
size_t vib_line(const uint8_t *pkt, size_t len, char *out);

#endif /* ATRIUM_VIB_SPI_H_ */
