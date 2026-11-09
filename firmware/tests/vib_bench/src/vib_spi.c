/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "vib_spi.h"

#include <string.h>

int vib_spi_write(struct vib_spi *s, uint8_t reg, uint8_t val)
{
	s->tx[0] = reg & 0x7F;
	s->tx[1] = val;
	int r = s->xfer(s->ctx, s->tx, s->rx, 2);

	s->st.errors += r != 0;
	return r;
}

int vib_spi_read(struct vib_spi *s, uint8_t reg, uint8_t *out, size_t n)
{
	if (n == 0 || n + 1 > VIB_SPI_BUF) {
		return -1;
	}
	memset(s->tx, 0, n + 1);
	s->tx[0] = VIB_SPI_READ | reg;
	int r = s->xfer(s->ctx, s->tx, s->rx, n + 1);

	s->st.errors += r != 0;
	if (r == 0 && out != s->rx + 1) {
		memcpy(out, s->rx + 1, n);
	}
	return r;
}

int vib_spi_probe(struct vib_spi *s)
{
	uint8_t id = 0;

	if (vib_spi_read(s, VIB_SPI_WHO_AM_I, &id, 1)) {
		return -1;
	}
	return id == VIB_SPI_WHO_AM_I_ID ? 0 : -2;
}

int vib_spi_start(struct vib_spi *s)
{
	struct vib_reg w[VIB_MAX_WRITES];
	size_t n = vib_transition(MODE_FULL, MODE_RESEARCH_VIB6K7, w);
	int ret = 0;

	ret |= vib_spi_write(s, VIB_SPI_CTRL4_C, VIB_SPI_CTRL4_RUN);
	ret |= vib_spi_write(s, VIB_SPI_CTRL3_C, VIB_SPI_CTRL3_RUN);
	for (size_t i = 0; i < n; i++) {
		ret |= vib_spi_write(s, w[i].reg, w[i].val);
	}
	memset(&s->st, 0, sizeof(s->st));
	s->stopped = false;
	vib_cap_start(&s->cap, MODE_RESEARCH_VIB6K7, VIB_SPI_AXIS, VIB_SPI_NEG, VIB_MAX_PKT);
	return ret ? -1 : 0;
}

int vib_spi_poll(struct vib_spi *s, vib_emit_t emit, void *user)
{
	uint8_t st[2];

	if (vib_spi_read(s, VIB_REG_FIFO_STATUS1, st, 2)) {
		return -1;
	}
	uint64_t t_us = s->now_us(s->ctx);
	struct vib_status fs = vib_parse_status(st);
	size_t n = fs.level < VIB_BURST_WORDS ? fs.level : VIB_BURST_WORDS;
	int got = 0;

	s->st.polls++;
	if (fs.level > s->st.max_level) {
		s->st.max_level = fs.level;
	}
	s->stopped |= fs.stopped;
	size_t n_read = fs.wtm || s->stopped ? n : 0;
	/* a stopped FIFO's words date from when it filled, not from now */
	uint64_t t_new = vib_cap_clock(&s->cap, &fs, s->stopped, n_read, t_us);

	if (n_read) {
		if (vib_spi_read(s, VIB_REG_FIFO_OUT, s->rx + 1, n * VIB_WORD)) {
			return -1;
		}
		vib_cap_burst(&s->cap, s->rx + 1, n, fs.level, t_new, emit, user);
		s->st.bursts++;
		s->st.words += n;
		got = (int)n;
	}
	if (s->stopped && fs.level <= n) {
		struct vib_reg w[2];
		size_t k = vib_restart(w);

		for (size_t i = 0; i < k; i++) {
			(void)vib_spi_write(s, w[i].reg, w[i].val);
		}
		vib_cap_gap(&s->cap, emit, user);
		s->st.gaps++;
		s->stopped = false;
	}
	return got;
}

size_t vib_line(const uint8_t *pkt, size_t len, char *out)
{
	static const char hex[] = "0123456789abcdef";
	size_t k = 0;

	memcpy(out, "0207 ", 5);
	k = 5;
	for (size_t i = 0; i < len; i++) {
		out[k++] = hex[pkt[i] >> 4];
		out[k++] = hex[pkt[i] & 0x0F];
	}
	out[k++] = '\n';
	return k;
}
