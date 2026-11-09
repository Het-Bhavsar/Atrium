/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Vibration capture, research modes 10 and 11 (vibcap.h, docs/ble_gatt.md 4.4).
 */
#include <string.h>
#include <zephyr/ztest.h>

#include "vibcap.h"

ZTEST_SUITE(vibcap, NULL, NULL, NULL, NULL, NULL);

/* ---- packets from the capture ---- */

#define MAX_PKTS 400

static struct {
	struct vib_hdr h[MAX_PKTS];
	int16_t s[MAX_PKTS][VIB_MAX_SAMPLES];
	int n[MAX_PKTS];
	size_t len[MAX_PKTS];
	int count;
} got;

static void collect(const uint8_t *pkt, size_t len, void *user)
{
	ARG_UNUSED(user);
	zassert_true(got.count < MAX_PKTS);
	zassert_true(len <= VIB_MAX_PKT, "packet %u B", (unsigned)len);
	got.len[got.count] = len;
	got.n[got.count] = vib_decode(pkt, len, &got.h[got.count], got.s[got.count],
				      VIB_MAX_SAMPLES);
	zassert_true(got.n[got.count] > 0);
	got.count++;
}

/* A FIFO word: accelerometer tag, then x, y, z little-endian. */
static void word(uint8_t *w, uint8_t tag, int16_t x, int16_t y, int16_t z)
{
	int16_t v[3] = {x, y, z};

	w[0] = (uint8_t)(tag << 3);
	for (int i = 0; i < 3; i++) {
		w[1 + 2 * i] = (uint8_t)v[i];
		w[2 + 2 * i] = (uint8_t)((uint16_t)v[i] >> 8);
	}
}

/* The LSM6DSOX at `hz_mhz`: sample j is taken at t0_us + j x period; z carries j. A drain at
 * t_us reads up to VIB_BURST_WORDS of the words taken and not yet read. */
struct imu_sim {
	uint32_t mhz;
	uint64_t t0_us;
	uint32_t taken_read; /* words read so far */
};

static uint32_t taken(const struct imu_sim *m, uint64_t t_us)
{
	return t_us < m->t0_us ? 0 : (uint32_t)((t_us - m->t0_us) * m->mhz / 1000000000ULL) + 1;
}

static size_t drain(struct imu_sim *m, struct vib_cap *c, uint64_t t_us)
{
	uint8_t raw[VIB_BURST_WORDS * VIB_WORD];
	uint16_t level = (uint16_t)(taken(m, t_us) - m->taken_read);
	size_t n = MIN(level, VIB_BURST_WORDS);

	for (size_t i = 0; i < n; i++) {
		uint32_t j = m->taken_read + (uint32_t)i;

		word(&raw[i * VIB_WORD], VIB_TAG_XL_NC, 1000, -2000, (int16_t)(j & 0x7FFF));
	}
	m->taken_read += (uint32_t)n;
	vib_cap_burst(c, raw, n, level, t_us, collect, NULL);
	return n;
}

ZTEST(vibcap, test_packet_round_trip)
{
	struct vib_hdr h = {.seq = 0xBEEF, .rate_hz = 6667, .t_ms = 0x01020304,
			    .index = 0xA0B0C0D0, .mode = 11, .flags = VIB_FLAG_GAP};
	int16_t s[VIB_MAX_SAMPLES];
	uint8_t pkt[VIB_MAX_PKT];
	struct vib_hdr d;
	int16_t back[VIB_MAX_SAMPLES];

	for (int i = 0; i < VIB_MAX_SAMPLES; i++) {
		s[i] = (int16_t)(i * 571 - 32000);
	}
	s[0] = INT16_MIN;
	s[1] = INT16_MAX;
	size_t len = vib_encode(&h, s, VIB_MAX_SAMPLES, pkt);

	/* the whole packet fits a 247-byte MTU notification */
	zassert_equal(len, 244);
	/* the layout of docs/ble_gatt.md 4.4, little-endian */
	zassert_equal(pkt[0], 0xEF);
	zassert_equal(pkt[1], 0xBE);
	zassert_equal(pkt[2] | (pkt[3] << 8), 6667);
	zassert_equal(pkt[4], 0x04);
	zassert_equal(pkt[7], 0x01);
	zassert_equal(pkt[8], 0xD0);
	zassert_equal(pkt[11], 0xA0);
	zassert_equal(pkt[12], 11);
	zassert_equal(pkt[13], VIB_FLAG_GAP);
	zassert_equal(pkt[14], 0x00); /* INT16_MIN = 0x8000 */
	zassert_equal(pkt[15], 0x80);

	zassert_equal(vib_decode(pkt, len, &d, back, VIB_MAX_SAMPLES), VIB_MAX_SAMPLES);
	zassert_equal(d.seq, h.seq);
	zassert_equal(d.rate_hz, h.rate_hz);
	zassert_equal(d.t_ms, h.t_ms);
	zassert_equal(d.index, h.index);
	zassert_equal(d.mode, h.mode);
	zassert_equal(d.flags, h.flags);
	zassert_mem_equal(back, s, sizeof(s));
	/* malformed: short header, odd sample bytes, more samples than room */
	zassert_equal(vib_decode(pkt, VIB_HDR_LEN - 1, &d, back, VIB_MAX_SAMPLES), -1);
	zassert_equal(vib_decode(pkt, VIB_HDR_LEN + 3, &d, back, VIB_MAX_SAMPLES), -1);
	zassert_equal(vib_decode(pkt, len, &d, back, 10), -1);
	zassert_equal(vib_decode(pkt, VIB_HDR_LEN, &d, back, 0), 0);
}

/* Sequence, sample index, values and time stay continuous across irregular bursts. */
static void run_continuity(enum pod_mode mode, uint32_t mhz, uint32_t dur_us)
{
	struct vib_cap c;
	struct imu_sim m = {.mhz = mhz, .t0_us = 5000000};
	uint64_t t = m.t0_us;
	uint32_t i;

	memset(&got, 0, sizeof(got));
	vib_cap_start(&c, mode, 2, false, 244);
	zassert_equal(c.max, VIB_MAX_SAMPLES);
	/* polls 3 ms apart plus the burst time, drains whenever a watermark is in. At 6667 Hz
	 * the bus falls behind (40 words per 6 ms burst of 36), so that run stays short of the
	 * 3 kB FIFO */
	while (t < m.t0_us + dur_us) {
		t += 3000 + (t / 7) % 900;
		while (t < m.t0_us + dur_us && taken(&m, t) - m.taken_read >= VIB_WTM) {
			drain(&m, &c, t);
			t += 6000; /* 36 words at 400 kHz: about 6 ms on the bus */
		}
	}
	zassert_true(got.count > 20);
	uint32_t next = 0;

	for (i = 0; i < (uint32_t)got.count; i++) {
		struct vib_hdr *h = &got.h[i];

		zassert_equal(h->seq, (uint16_t)i, "sequence +1 per packet");
		zassert_equal(h->index, next, "packet %u: index %u, expected %u", i, h->index, next);
		zassert_equal(h->mode, mode);
		zassert_equal(h->rate_hz, vib_rate_hz(mode));
		zassert_equal(h->flags, 0);
		zassert_equal(got.n[i], VIB_MAX_SAMPLES, "full packets while streaming");
		for (int k = 0; k < got.n[i]; k++) {
			zassert_equal(got.s[i][k], (int16_t)((next + k) & 0x7FFF), "axis z, in order");
		}
		/* the first sample's time against the true clock: within one ms (rounding) */
		uint64_t true_us = m.t0_us + (uint64_t)h->index * 1000000000ULL / mhz;
		int64_t err = (int64_t)h->t_ms * 1000 - (int64_t)true_us;

		zassert_true(err > -1500 && err < 1000, "packet %u: %lld us off", i, (long long)err);
		next += (uint32_t)got.n[i];
	}
}

ZTEST(vibcap, test_continuity_across_fifo_bursts)
{
	run_continuity(MODE_RESEARCH_VIB3K3, 3333333, 2000000);
	run_continuity(MODE_RESEARCH_VIB6K7, 6666667, 500000);
}

ZTEST(vibcap, test_fifo_full_gap_is_flagged)
{
	struct vib_cap c;
	struct imu_sim m = {.mhz = 6666667, .t0_us = 1000000};
	uint64_t t = m.t0_us + 20000;

	memset(&got, 0, sizeof(got));
	vib_cap_start(&c, MODE_RESEARCH_VIB6K7, 2, false, 244);
	drain(&m, &c, t);
	drain(&m, &c, t + 6000);
	uint32_t before = c.index;
	int sent = got.count;

	/* the FIFO filled and was restarted 100 ms later: what was held goes out short */
	vib_cap_gap(&c, collect, NULL);
	zassert_equal(got.count, sent + 1);
	zassert_true(got.n[sent] < VIB_MAX_SAMPLES);
	zassert_equal(got.h[sent].flags, 0);
	zassert_equal(got.h[sent].index + (uint32_t)got.n[sent], before);

	/* a new window: the index runs on (it counts samples from the FIFO), the time jumps */
	m.t0_us = t + 100000;
	m.taken_read = 0;
	t = m.t0_us + 30000;
	while (got.count < sent + 3) {
		drain(&m, &c, t);
		t += 6000;
	}
	struct vib_hdr *h = &got.h[sent + 1];

	zassert_equal(h->flags, VIB_FLAG_GAP, "the first packet after the restart");
	zassert_equal(h->index, before);
	zassert_true(h->t_ms * 1000ULL >= m.t0_us - 1000, "time after the gap");
	zassert_equal(got.h[sent + 2].flags, 0, "only once");
	zassert_equal(got.h[sent + 2].seq, (uint16_t)(sent + 2));
}

/* Read `n` of the `level` unread words, passing `t_pass` as the burst time. */
static void burst(struct imu_sim *m, struct vib_cap *c, uint16_t level, size_t n, uint64_t t_pass)
{
	uint8_t raw[VIB_BURST_WORDS * VIB_WORD];

	for (size_t i = 0; i < n; i++) {
		uint32_t j = m->taken_read + (uint32_t)i;

		word(&raw[i * VIB_WORD], VIB_TAG_XL_NC, 0, 0, (int16_t)(j & 0x7FFF));
	}
	m->taken_read += (uint32_t)n;
	vib_cap_burst(c, raw, n, level, t_pass, collect, NULL);
}

/* FIFO mode stops when full, so the words drained after the stop were made before it: their
 * packets keep their true times (vib_cap_clock), not the read time. */
ZTEST(vibcap, test_stopped_fifo_keeps_true_time)
{
	const uint16_t cap = 438; /* 3 kB / 7 B [VERIFY DS12814 FIFO size] */
	struct vib_cap c;
	struct imu_sim m = {.mhz = 6666667, .t0_us = 1000000};
	uint64_t t = m.t0_us + 20000;

	memset(&got, 0, sizeof(got));
	vib_cap_start(&c, MODE_RESEARCH_VIB6K7, 2, false, 244);
	/* running: one burst at the watermark */
	struct vib_status fs = {.level = (uint16_t)(taken(&m, t) - m.taken_read), .wtm = true};

	zassert_equal(vib_cap_clock(&c, &fs, false, VIB_BURST_WORDS, t), t, "running: read time");
	burst(&m, &c, fs.level, VIB_BURST_WORDS, t);

	/* held off 100 ms: the FIFO filled about 35 ms ago and stopped */
	t += 100000;
	uint32_t newest = m.taken_read + cap - 1;
	uint64_t newest_us = m.t0_us + (uint64_t)newest * 1000000000ULL / m.mhz;

	zassert_true(t > newest_us + 30000);
	bool stopped = true;
	uint16_t level = cap;

	while (level) {
		size_t n = MIN(level, VIB_BURST_WORDS);

		fs = (struct vib_status){.level = level, .wtm = true, .stopped = true};
		uint64_t tn = vib_cap_clock(&c, &fs, stopped, n, t);

		zassert_true(tn <= newest_us + 200 && tn + 200 >= newest_us,
			     "stop time %llu, newest word %llu", (unsigned long long)tn,
			     (unsigned long long)newest_us);
		burst(&m, &c, level, n, tn);
		level -= (uint16_t)n;
		t += 6000;
	}
	zassert_true(got.count >= 3);
	for (int i = 0; i < got.count; i++) {
		uint64_t true_us = m.t0_us + (uint64_t)got.h[i].index * 1000000000ULL / m.mhz;
		int64_t err = (int64_t)got.h[i].t_ms * 1000 - (int64_t)true_us;

		zassert_true(err > -1500 && err < 1000, "packet %d: %lld us off", i, (long long)err);
	}
	/* the restart: what was held goes out, the next packet carries the gap flag */
	vib_cap_gap(&c, collect, NULL);
	int last = got.count - 1;
	uint32_t index_at_gap = c.index;

	zassert_equal(got.h[last].index + (uint32_t)got.n[last], newest + 1);

	/* a new window from the restart: its first sample is made one period after it */
	uint64_t restart_us = t;

	m.t0_us = restart_us + 150;
	m.taken_read = 0;
	t = m.t0_us + 10000;
	fs = (struct vib_status){.level = (uint16_t)(taken(&m, t) - m.taken_read), .wtm = true};
	uint64_t tn = vib_cap_clock(&c, &fs, false, VIB_BURST_WORDS, t);

	zassert_equal(tn, t, "running again: the read time");
	while (got.count < last + 2 + 1) {
		fs.level = (uint16_t)(taken(&m, t) - m.taken_read);
		burst(&m, &c, fs.level, VIB_BURST_WORDS,
		      vib_cap_clock(&c, &fs, false, VIB_BURST_WORDS, t));
		t += 6000;
	}
	struct vib_hdr *a = &got.h[last + 1];

	zassert_equal(a->flags, VIB_FLAG_GAP);
	zassert_equal(a->index, index_at_gap, "the index runs on with no jump");
	zassert_true(a->t_ms * 1000LL > (int64_t)m.t0_us - 1500 && a->t_ms * 1000LL < (int64_t)m.t0_us + 1000,
		     "first sample after the gap at %u ms, made at %llu us", a->t_ms,
		     (unsigned long long)m.t0_us);
	/* the gap as the decoder works it out: the uptime jump less the expected step */
	int64_t step_us = (int64_t)got.n[last] * 150;
	int64_t gap_us = ((int64_t)a->t_ms - (int64_t)got.h[last].t_ms) * 1000 - step_us;
	int64_t true_gap_us = (int64_t)m.t0_us - (int64_t)(newest_us + 150);

	zassert_true(gap_us > true_gap_us - 2000 && gap_us < true_gap_us + 2000,
		     "gap %lld us, true %lld us", (long long)gap_us, (long long)true_gap_us);
}

ZTEST(vibcap, test_axis_sign_and_skipped_words)
{
	struct vib_cap c;
	uint8_t raw[4 * VIB_WORD];

	memset(&got, 0, sizeof(got));
	/* small MTU: 23 - 3 = 20 B of payload, so 3 samples per packet */
	vib_cap_start(&c, MODE_RESEARCH_VIB3K3, 1, true, 20);
	zassert_equal(c.max, 3);
	word(&raw[0], VIB_TAG_XL_NC, 1, 100, 7);
	word(&raw[VIB_WORD], 0x05, 2, 200, 8); /* CFG_Change: not a sample */
	word(&raw[2 * VIB_WORD], VIB_TAG_XL_NC, 3, INT16_MIN, 9);
	word(&raw[3 * VIB_WORD], VIB_TAG_XL_NC, 4, -300, 10);
	vib_cap_burst(&c, raw, 4, 4, 2000000, collect, NULL);
	zassert_equal(got.count, 1);
	zassert_equal(got.n[0], 3);
	zassert_equal(got.s[0][0], -100, "pod axis = -chip y");
	zassert_equal(got.s[0][1], INT16_MAX, "-(-32768) saturates");
	zassert_equal(got.s[0][2], 300);
	zassert_equal(got.h[0].flags, VIB_FLAG_SKIPPED);
	zassert_equal(c.index, 3, "skipped words take no index");
}

ZTEST(vibcap, test_status_parse)
{
	struct vib_status s = vib_parse_status((const uint8_t[]){0x34, 0x81});

	zassert_equal(s.level, 0x134);
	zassert_true(s.wtm);
	zassert_false(s.stopped);
	/* OVR_IA or OVR_LATCHED: filled; FULL_IA alone (full at the next ODR) is not yet loss */
	zassert_true(vib_parse_status((const uint8_t[]){0, 0x40}).stopped);
	zassert_true(vib_parse_status((const uint8_t[]){0, 0x08}).stopped);
	zassert_false(vib_parse_status((const uint8_t[]){0, 0x20}).stopped);
	zassert_equal(vib_parse_status((const uint8_t[]){0xFF, 0x03}).level, 1023);
}

/* ---- register writes: in and out of the vibration modes ---- */

struct imu_regs {
	uint8_t r[0x80];
};

static void apply(struct imu_regs *s, const struct vib_reg *w, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		s->r[w[i].reg] = w[i].val;
	}
}

/* The Zephyr driver's part of imu_apply: ODR_XL by read-modify-write, nothing else. */
static void driver_odr(struct imu_regs *s, uint32_t mhz)
{
	uint8_t code = mhz == 0 ? 0 : mhz <= 12500 ? 1 : mhz <= 104000 ? 4 : 0xF;

	s->r[VIB_REG_CTRL1_XL] = (uint8_t)((s->r[VIB_REG_CTRL1_XL] & 0x0F) | (code << 4));
}

static void set_mode(struct imu_regs *s, enum pod_mode from, enum pod_mode to)
{
	struct vib_reg w[VIB_MAX_WRITES];
	size_t n = vib_transition(from, to, w);

	zassert_true(n <= VIB_MAX_WRITES);
	if (n) {
		/* the accelerometer is powered down before anything else changes */
		zassert_equal(w[0].reg, VIB_REG_CTRL1_XL, "%d -> %d", from, to);
		zassert_equal(w[0].val, 0);
	} else {
		zassert_false(mode_is_vibration(from) || mode_is_vibration(to));
	}
	apply(s, w, n);
	if (!mode_is_vibration(to)) {
		driver_odr(s, mode_get(to)->imu_mhz);
	}
}

static void check_mode(const struct imu_regs *s, enum pod_mode m)
{
	uint8_t odr = s->r[VIB_REG_CTRL1_XL] >> 4;

	if (mode_is_vibration(m)) {
		zassert_equal(odr, vib_odr_code(m), "%s", mode_get(m)->name);
		zassert_equal(s->r[VIB_REG_CTRL1_XL] & 0x0E, 0, "+-2 g, LPF2 off");
		zassert_equal(s->r[VIB_REG_CTRL6_C] & VIB_CTRL6_LP_NORMAL, 0, "high-performance");
		zassert_equal(s->r[VIB_REG_FIFO_CTRL3], vib_odr_code(m), "accelerometer only");
		zassert_equal(s->r[VIB_REG_FIFO_CTRL4] & 0x07, VIB_FIFO_FIFO_MODE);
		zassert_equal(s->r[VIB_REG_FIFO_CTRL1], VIB_WTM);
		return;
	}
	/* never left at a kHz rate, in high-performance, or batching */
	zassert_true(odr <= 4, "%s: ODR code %u", mode_get(m)->name, odr);
	zassert_equal(s->r[VIB_REG_CTRL6_C] & VIB_CTRL6_LP_NORMAL, VIB_CTRL6_LP_NORMAL, "%s",
		      mode_get(m)->name);
	zassert_equal(s->r[VIB_REG_FIFO_CTRL4] & 0x07, 0, "%s: bypass", mode_get(m)->name);
	zassert_equal(s->r[VIB_REG_FIFO_CTRL3], 0, "%s", mode_get(m)->name);
}

ZTEST(vibcap, test_mode_switch_in_and_out)
{
	/* every pair: from the state the first mode leaves to the second */
	for (int a = 0; a < MODE_COUNT; a++) {
		for (int b = 0; b < MODE_COUNT; b++) {
			struct imu_regs s;

			memset(&s, 0, sizeof(s));
			s.r[VIB_REG_CTRL6_C] = VIB_CTRL6_LP_NORMAL; /* after the driver's init */
			set_mode(&s, MODE_SMART, a);
			check_mode(&s, a);
			set_mode(&s, a, b);
			check_mode(&s, b);
		}
	}
	/* the restart after a full FIFO: bypass empties it, then FIFO mode */
	struct vib_reg w[2];

	zassert_equal(vib_restart(w), 2);
	zassert_equal(w[0].reg, VIB_REG_FIFO_CTRL4);
	zassert_equal(w[0].val, 0);
	zassert_equal(w[1].val, VIB_FIFO_FIFO_MODE);
	zassert_equal(vib_odr_code(MODE_RESEARCH_200), 0);
}

ZTEST(vibcap, test_stream_size)
{
	static const struct {
		enum pod_mode mode;
		uint16_t hz;
		uint16_t bps;
	} v[] = {{MODE_RESEARCH_VIB3K3, 3333, 6667}, {MODE_RESEARCH_VIB6K7, 6667, 13333}};

	for (size_t i = 0; i < ARRAY_SIZE(v); i++) {
		const struct mode_cfg *m = mode_get(v[i].mode);

		zassert_equal(vib_rate_hz(v[i].mode), v[i].hz);
		/* 2 bytes per sample at the ODR */
		zassert_equal(m->stream_bps, v[i].bps);
		zassert_equal(m->stream_bps, (m->imu_mhz * 2 + 500) / 1000);
		/* with 115 samples and a 14-byte header per notification */
		uint32_t pkts = (m->imu_mhz + VIB_MAX_SAMPLES * 1000 - 1) / (VIB_MAX_SAMPLES * 1000);

		zassert_true(m->stream_bps + pkts * VIB_HDR_LEN < (i ? 14200 : 7100));
	}
}
