/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "vibcap.h"

#include <string.h>

#define ODR_3333 0x9
#define ODR_6667 0xA

static void put16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
	put16(p, (uint16_t)v);
	put16(p + 2, (uint16_t)(v >> 16));
}

static uint16_t get16(const uint8_t *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get32(const uint8_t *p)
{
	return get16(p) | ((uint32_t)get16(p + 2) << 16);
}

uint8_t vib_odr_code(enum pod_mode m)
{
	return m == MODE_RESEARCH_VIB3K3 ? ODR_3333 : m == MODE_RESEARCH_VIB6K7 ? ODR_6667 : 0;
}

uint16_t vib_rate_hz(enum pod_mode m)
{
	return m == MODE_RESEARCH_VIB3K3 ? 3333 : m == MODE_RESEARCH_VIB6K7 ? 6667 : 0;
}

size_t vib_transition(enum pod_mode from, enum pod_mode to, struct vib_reg out[VIB_MAX_WRITES])
{
	size_t n = 0;
	uint8_t odr = vib_odr_code(to);

	if (mode_is_vibration(from) || odr) {
		/* accelerometer off before anything else changes */
		out[n++] = (struct vib_reg){VIB_REG_CTRL1_XL, 0x00};
		out[n++] = (struct vib_reg){VIB_REG_FIFO_CTRL4, 0x00};
	}
	if (!odr) {
		if (n) {
			out[n++] = (struct vib_reg){VIB_REG_FIFO_CTRL3, 0x00};
			out[n++] = (struct vib_reg){VIB_REG_CTRL6_C, VIB_CTRL6_LP_NORMAL};
		}
		return n;
	}
	out[n++] = (struct vib_reg){VIB_REG_CTRL6_C, 0x00};
	out[n++] = (struct vib_reg){VIB_REG_FIFO_CTRL1, VIB_WTM};
	out[n++] = (struct vib_reg){VIB_REG_FIFO_CTRL2, 0x00};
	out[n++] = (struct vib_reg){VIB_REG_FIFO_CTRL3, odr}; /* accelerometer only, no gyro */
	out[n++] = (struct vib_reg){VIB_REG_FIFO_CTRL4, VIB_FIFO_FIFO_MODE};
	/* ODR, FS_XL 00 (+-2 g, the MLC program's and the driver's), LPF2_XL_EN 0 */
	out[n++] = (struct vib_reg){VIB_REG_CTRL1_XL, (uint8_t)(odr << 4)};
	return n;
}

size_t vib_restart(struct vib_reg out[2])
{
	out[0] = (struct vib_reg){VIB_REG_FIFO_CTRL4, 0x00};
	out[1] = (struct vib_reg){VIB_REG_FIFO_CTRL4, VIB_FIFO_FIFO_MODE};
	return 2;
}

struct vib_status vib_parse_status(const uint8_t st[2])
{
	/* FIFO_STATUS2: 7 WTM_IA, 6 OVR_IA (completely filled), 5 FULL_IA (full at the next
	 * ODR), 3 OVR_LATCHED (cleared by the read), 1:0 DIFF_FIFO[9:8] */
	return (struct vib_status){
		.level = (uint16_t)(st[0] | ((st[1] & 0x03) << 8)),
		.wtm = (st[1] & 0x80) != 0,
		.stopped = (st[1] & 0x48) != 0,
	};
}

size_t vib_encode(const struct vib_hdr *h, const int16_t *s, size_t n, uint8_t *out)
{
	put16(out, h->seq);
	put16(out + 2, h->rate_hz);
	put32(out + 4, h->t_ms);
	put32(out + 8, h->index);
	out[12] = h->mode;
	out[13] = h->flags;
	for (size_t i = 0; i < n; i++) {
		put16(out + VIB_HDR_LEN + 2 * i, (uint16_t)s[i]);
	}
	return VIB_HDR_LEN + 2 * n;
}

int vib_decode(const uint8_t *p, size_t len, struct vib_hdr *h, int16_t *s, size_t max)
{
	if (len < VIB_HDR_LEN || (len - VIB_HDR_LEN) % 2 || (len - VIB_HDR_LEN) / 2 > max) {
		return -1;
	}
	h->seq = get16(p);
	h->rate_hz = get16(p + 2);
	h->t_ms = get32(p + 4);
	h->index = get32(p + 8);
	h->mode = p[12];
	h->flags = p[13];
	size_t n = (len - VIB_HDR_LEN) / 2;

	for (size_t i = 0; i < n; i++) {
		s[i] = (int16_t)get16(p + VIB_HDR_LEN + 2 * i);
	}
	return (int)n;
}

void vib_cap_start(struct vib_cap *c, enum pod_mode m, uint8_t axis, bool neg, size_t room)
{
	const struct mode_cfg *cfg = mode_get(m);
	size_t max = room > VIB_HDR_LEN ? (room - VIB_HDR_LEN) / 2 : 1;

	memset(c, 0, sizeof(*c));
	c->mode = (uint8_t)m;
	c->axis = axis < 3 ? axis : 2;
	c->neg = neg;
	c->max = (uint8_t)(max > VIB_MAX_SAMPLES ? VIB_MAX_SAMPLES : max ? max : 1);
	c->hz = vib_rate_hz(m);
	c->imu_mhz = cfg ? cfg->imu_mhz : 0;
}

static void send(struct vib_cap *c, vib_emit_t emit, void *user)
{
	uint8_t pkt[VIB_MAX_PKT];
	struct vib_hdr h = {
		.seq = c->seq++,
		.rate_hz = c->hz,
		.t_ms = (uint32_t)(c->buf_t_us / 1000U),
		.index = c->buf_index,
		.mode = c->mode,
		.flags = c->flags,
	};

	if (emit) {
		emit(pkt, vib_encode(&h, c->buf, c->n, pkt), user);
	}
	c->n = 0;
	c->flags = 0;
}

/* sample period in us, x 1000 for the arithmetic: 1e12 / mHz */
static uint64_t period_ns(const struct vib_cap *c)
{
	return c->imu_mhz ? 1000000000000ULL / c->imu_mhz : 0;
}

void vib_cap_burst(struct vib_cap *c, const uint8_t *raw, size_t n, uint16_t level,
		   uint64_t t_us, vib_emit_t emit, void *user)
{
	uint64_t per = period_ns(c);
	/* the newest word in the FIFO is about t_us old; the oldest, read first, level-1 before */
	uint64_t back_us = level ? (uint64_t)(level - 1) * per / 1000U : 0;
	uint64_t t0 = t_us > back_us ? t_us - back_us : 0;
	uint32_t k = 0;

	for (size_t w = 0; w < n; w++) {
		const uint8_t *p = raw + w * VIB_WORD;

		if ((p[0] >> 3) != VIB_TAG_XL_NC) {
			c->flags |= VIB_FLAG_SKIPPED;
			continue;
		}
		int32_t v = (int16_t)get16(p + 1 + 2 * c->axis);

		if (c->neg) {
			v = v == INT16_MIN ? INT16_MAX : -v;
		}
		if (c->n == 0) {
			c->buf_index = c->index;
			c->buf_t_us = t0 + (uint64_t)k * per / 1000U;
		}
		c->buf[c->n++] = (int16_t)v;
		c->index++;
		k++;
		if (c->n >= c->max) {
			send(c, emit, user);
		}
	}
}

uint64_t vib_cap_clock(struct vib_cap *c, const struct vib_status *fs, bool stopped,
		       size_t n_read, uint64_t t_us)
{
	if (!stopped) {
		c->clk_stopped = false;
		c->clk_last_us = t_us;
		c->clk_left = (uint16_t)(fs->level > n_read ? fs->level - n_read : 0);
		return t_us;
	}
	if (!c->clk_stopped) {
		/* full at the first stopped status: the words since the last running one came
		 * one period apart after it */
		uint16_t since = fs->level > c->clk_left ? fs->level - c->clk_left : 0;
		uint64_t t = c->clk_last_us ? c->clk_last_us + (uint64_t)since * period_ns(c) / 1000U
					    : t_us; /* no running status yet: the read time */

		c->clk_stop_us = t < t_us ? t : t_us;
		c->clk_stopped = true;
	}
	return c->clk_stop_us;
}

void vib_cap_gap(struct vib_cap *c, vib_emit_t emit, void *user)
{
	if (c->n) {
		send(c, emit, user);
	}
	c->flags |= VIB_FLAG_GAP;
}
