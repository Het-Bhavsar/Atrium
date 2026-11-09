/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * The bench rig's SPI drain (vib_spi.c) against a simulated LSM6DSOX: register file, FIFO of
 * 438 words (3 kB / 7 B [VERIFY DS12814 FIFO size]) filled at 6667 Hz in FIFO mode (stops when
 * full), burst reads that roll from 0x7E back to 0x78, and SPI time at 8 MHz plus a fixed
 * per-transfer overhead. The drain runs as main.c runs it: poll, sleep 1 ms when idle.
 */
#include <zephyr/ztest.h>

#include <stdlib.h>
#include <string.h>

#include "vib_spi.h"

#define CAP          438
#define PERIOD_NS    150000ULL /* 1e12 / 6666667 mHz, the mode table's rate */
#define BYTE_NS      1000ULL   /* 8 MHz SPI */
#define XFER_OVH_NS  40000ULL  /* driver call, chip-select and DMA set-up [ASSUMED, generous] */
#define IDLE_NS      1000000ULL

struct sim {
	uint64_t t_ns;
	uint64_t gen_next_ns; /* time of the next sample */
	uint64_t gen_t0_ns;   /* time of sample 0 */
	bool gen_on;
	uint32_t k;           /* samples the chip has made since it started */
	uint32_t fifo[CAP];   /* sample numbers */
	uint32_t head, count;
	bool stopped, ovr_latched;
	uint8_t regs[128];
	uint32_t cur;         /* the word being read out */
	bool cur_valid;
	uint32_t lost;
	uint32_t xfers, short_xfers;
	uint64_t bus_ns;
};

static struct sim sim;

static int16_t z_of(uint32_t k)
{
	return (int16_t)(k * 37u + 11u);
}

static void sim_run_to(uint64_t t)
{
	while (sim.gen_on && sim.gen_next_ns <= t) {
		bool running = (sim.regs[VIB_REG_FIFO_CTRL4] & 7) == VIB_FIFO_FIFO_MODE &&
			       (sim.regs[VIB_REG_FIFO_CTRL3] & 0x0F) == 0x0A;

		if (running && !sim.stopped && sim.count < CAP) {
			sim.fifo[(sim.head + sim.count) % CAP] = sim.k;
			sim.count++;
		} else {
			sim.lost++;
			if (running && sim.count >= CAP) {
				sim.stopped = true;
				sim.ovr_latched = true;
			}
		}
		sim.k++;
		sim.gen_next_ns += PERIOD_NS;
	}
	sim.t_ns = MAX(sim.t_ns, t);
}

static uint8_t sim_read(uint8_t a)
{
	if (a == VIB_REG_FIFO_STATUS1) {
		return (uint8_t)sim.count;
	}
	if (a == VIB_REG_FIFO_STATUS1 + 1) {
		uint8_t v = (uint8_t)((sim.count >> 8) & 3);

		v |= sim.count >= sim.regs[VIB_REG_FIFO_CTRL1] ? 0x80 : 0;
		v |= sim.stopped ? 0x40 : 0;
		v |= sim.ovr_latched ? 0x08 : 0;
		sim.ovr_latched = false;
		return v;
	}
	if (a >= VIB_REG_FIFO_OUT && a <= VIB_REG_FIFO_OUT + 6) {
		if (a == VIB_REG_FIFO_OUT) {
			sim.cur_valid = sim.count > 0;
			if (sim.cur_valid) {
				sim.cur = sim.fifo[sim.head];
				sim.head = (sim.head + 1) % CAP;
				sim.count--;
			}
			return sim.cur_valid ? (uint8_t)(VIB_TAG_XL_NC << 3) : 0;
		}
		int16_t z = z_of(sim.cur);
		int16_t v[3] = {(int16_t)-z, (int16_t)(sim.cur & 0xFF), z};
		uint8_t i = a - VIB_REG_FIFO_OUT - 1;
		uint16_t u = (uint16_t)v[i / 2];

		return i & 1 ? (uint8_t)(u >> 8) : (uint8_t)u;
	}
	return sim.regs[a];
}

static void sim_write(uint8_t a, uint8_t v)
{
	sim.regs[a] = v;
	if (a == VIB_REG_FIFO_CTRL4 && (v & 7) == 0) {
		sim.count = 0; /* bypass empties the FIFO */
		sim.stopped = false;
	}
	if (a == VIB_REG_CTRL1_XL) {
		bool on = (v >> 4) == 0x0A;

		if (on && !sim.gen_on) {
			sim.gen_t0_ns = sim.t_ns + PERIOD_NS;
			sim.gen_next_ns = sim.gen_t0_ns;
			sim.k = 0;
		}
		sim.gen_on = on;
	}
}

static int sim_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
	ARG_UNUSED(ctx);
	sim.xfers++;
	sim.short_xfers += len < 2;
	sim_run_to(sim.t_ns);
	uint8_t a = tx[0] & 0x7F;

	if (tx[0] & VIB_SPI_READ) {
		rx[0] = 0xFF;
		for (size_t i = 1; i < len; i++) {
			rx[i] = sim_read(a);
			/* auto-increment; the FIFO output block rolls back to its start */
			a = a == VIB_REG_FIFO_OUT + 6 ? VIB_REG_FIFO_OUT : (uint8_t)(a + 1);
		}
	} else {
		for (size_t i = 1; i < len; i++) {
			sim_write((uint8_t)(a + i - 1), tx[i]);
		}
	}
	uint64_t d = XFER_OVH_NS + len * BYTE_NS;

	sim.bus_ns += d;
	sim_run_to(sim.t_ns + d);
	return 0;
}

static uint64_t sim_now_us(void *ctx)
{
	ARG_UNUSED(ctx);
	return sim.t_ns / 1000;
}

/* what the host decoder sees */
struct rx_log {
	uint32_t pkts, samples, bad, gap_pkts, skipped_pkts;
	uint16_t next_seq;
	uint32_t next_index;
	int32_t worst_t_err_us;
	uint32_t value_err;
	uint32_t lost_at_gap; /* from the gap packet's sample values */
};

static struct rx_log rl;

static void on_packet(const uint8_t *pkt, size_t len, void *user)
{
	static char line[VIB_LINE_MAX + 1];
	static uint8_t back[VIB_MAX_PKT];
	struct vib_hdr h;
	int16_t s[VIB_MAX_SAMPLES];

	ARG_UNUSED(user);
	/* through the serial line format and back, as record_bench.py and capture.py see it */
	size_t n = vib_line(pkt, len, line);

	zassert_true(n <= VIB_LINE_MAX);
	zassert_mem_equal(line, "0207 ", 5);
	zassert_equal(line[n - 1], '\n');
	for (size_t i = 0; i < len; i++) {
		char b[3] = {line[5 + 2 * i], line[6 + 2 * i], 0};

		back[i] = (uint8_t)strtoul(b, NULL, 16);
	}
	int m = vib_decode(back, len, &h, s, VIB_MAX_SAMPLES);

	if (m < 0) {
		rl.bad++;
		return;
	}
	rl.bad += h.seq != rl.next_seq || h.index != rl.next_index || h.mode != 11 ||
		  h.rate_hz != 6667;
	rl.gap_pkts += (h.flags & VIB_FLAG_GAP) != 0;
	rl.skipped_pkts += (h.flags & VIB_FLAG_SKIPPED) != 0;
	if (h.flags & VIB_FLAG_GAP) {
		/* the chip's sample number of this packet's first sample, from its value */
		for (uint32_t k = h.index; k < h.index + 4 * CAP * 10; k++) {
			if (z_of(k) == s[0] && z_of(k + 1) == s[1] && z_of(k + 2) == s[2]) {
				rl.lost_at_gap = k - h.index;
				break;
			}
		}
	}
	/* chip sample number = index + samples lost in gaps so far */
	uint32_t k0 = h.index + rl.lost_at_gap;

	for (int i = 0; i < m; i++) {
		rl.value_err += s[i] != z_of(k0 + (uint32_t)i);
	}
	int64_t truth_us = (int64_t)((sim.gen_t0_ns + (uint64_t)k0 * PERIOD_NS) / 1000);
	int32_t err = (int32_t)((int64_t)h.t_ms * 1000 - truth_us);

	if (abs(err) > abs(rl.worst_t_err_us)) {
		rl.worst_t_err_us = err;
	}
	rl.next_seq = h.seq + 1;
	rl.next_index = h.index + (uint32_t)m;
	rl.pkts++;
	rl.samples += (uint32_t)m;
}

static struct vib_spi vs;

static void start(void)
{
	memset(&sim, 0, sizeof(sim));
	memset(&rl, 0, sizeof(rl));
	memset(&vs, 0, sizeof(vs));
	sim.t_ns = 30000000; /* 30 ms after boot */
	sim.regs[VIB_SPI_WHO_AM_I] = VIB_SPI_WHO_AM_I_ID;
	sim.regs[VIB_REG_CTRL6_C] = 0x00;
	vs.xfer = sim_xfer;
	vs.now_us = sim_now_us;
	zassert_ok(vib_spi_probe(&vs));
	zassert_ok(vib_spi_start(&vs));
}

/* Drain for `dur_ns`, as main.c does; a `stall_ns` pause every `every_ns` stands for anything
 * that holds the drain off (other threads, interrupts). */
static void drain(uint64_t dur_ns, uint64_t stall_ns, uint64_t every_ns)
{
	uint64_t end = sim.t_ns + dur_ns;
	uint64_t next_stall = every_ns ? sim.t_ns + every_ns : UINT64_MAX;

	while (sim.t_ns < end) {
		int n = vib_spi_poll(&vs, on_packet, NULL);

		zassert_true(n >= 0);
		if (n == 0) {
			sim_run_to(sim.t_ns + IDLE_NS);
		}
		if (sim.t_ns >= next_stall) {
			sim_run_to(sim.t_ns + stall_ns);
			next_stall += every_ns;
		}
	}
}

ZTEST(vib_spi, test_setup_is_mode_11)
{
	start();
	zassert_equal(sim.regs[VIB_SPI_CTRL4_C], 0x04, "I2C off");
	zassert_equal(sim.regs[VIB_SPI_CTRL3_C], 0x04, "auto-increment on");
	zassert_equal(sim.regs[VIB_REG_CTRL6_C], 0x00, "high-performance");
	zassert_equal(sim.regs[VIB_REG_FIFO_CTRL1], VIB_WTM);
	zassert_equal(sim.regs[VIB_REG_FIFO_CTRL2], 0x00);
	zassert_equal(sim.regs[VIB_REG_FIFO_CTRL3], 0x0A, "accelerometer only at 6667 Hz");
	zassert_equal(sim.regs[VIB_REG_FIFO_CTRL4], VIB_FIFO_FIFO_MODE);
	zassert_equal(sim.regs[VIB_REG_CTRL1_XL], 0xA0, "6667 Hz, +-2 g, LPF2 off");
	zassert_equal(sim.regs[0x11], 0x00, "gyro off");
	zassert_equal(vs.cap.max, VIB_MAX_SAMPLES);
	zassert_equal(vs.cap.axis, 2);
	zassert_false(vs.cap.neg);
	zassert_equal(sim.short_xfers, 0, "no 1-byte transfer (anomaly 58)");
}

ZTEST(vib_spi, test_gap_free_6667hz_60s)
{
	start();
	/* a 3 ms hold-off every 17 ms on top of the 1 ms idle sleep */
	drain(60ULL * 1000000000ULL, 3000000, 17000000);

	uint32_t made = sim.k;

	TC_PRINT("60 s: %u samples made, %u sent in %u packets, max FIFO %u of %u words, "
		 "bus busy %u.%u %%, worst time error %d us\n",
		 made, rl.samples, rl.pkts, vs.st.max_level, CAP,
		 (unsigned int)(sim.bus_ns * 1000 / (60ULL * 1000000000ULL) / 10),
		 (unsigned int)(sim.bus_ns * 1000 / (60ULL * 1000000000ULL) % 10),
		 rl.worst_t_err_us);
	zassert_equal(sim.lost, 0, "no sample lost");
	zassert_equal(vs.st.gaps, 0);
	zassert_equal(rl.gap_pkts, 0);
	zassert_equal(rl.skipped_pkts, 0);
	zassert_equal(rl.bad, 0, "seq, index, mode and rate continuous");
	zassert_equal(rl.value_err, 0, "chip +Z, unnegated, in order");
	zassert_equal(rl.samples + vs.cap.n + sim.count, made, "every sample out or in flight");
	zassert_true(made >= 6666 * 60);
	zassert_true(abs(rl.worst_t_err_us) <= 1200, "%d", rl.worst_t_err_us);
	zassert_true(vs.st.max_level < CAP / 4, "%u", vs.st.max_level);
	zassert_equal(sim.short_xfers, 0);
}

ZTEST(vib_spi, test_50ms_hold_off_still_gap_free)
{
	start();
	/* the FIFO holds 438 words, 65.7 ms: a 50 ms hold-off once a second loses nothing */
	drain(5ULL * 1000000000ULL, 50000000, 1000000000);
	zassert_equal(sim.lost, 0);
	zassert_equal(rl.gap_pkts, 0);
	zassert_equal(rl.bad, 0);
	zassert_equal(rl.value_err, 0);
	TC_PRINT("50 ms hold-off: max FIFO %u of %u words\n", vs.st.max_level, CAP);
}

ZTEST(vib_spi, test_fifo_full_is_flagged)
{
	start();
	drain(500000000ULL, 0, 0);
	/* one 120 ms hold-off: the FIFO fills, stops, is drained and restarted */
	sim_run_to(sim.t_ns + 120000000ULL);
	drain(500000000ULL, 0, 0);
	zassert_equal(vs.st.gaps, 1);
	zassert_equal(rl.gap_pkts, 1, "one packet carries the gap flag");
	zassert_equal(rl.bad, 0, "the index runs on with no jump at a gap");
	zassert_true(sim.lost > 0);
	zassert_equal(rl.lost_at_gap, sim.lost, "the gap's length is what the chip dropped");
	zassert_equal(rl.value_err, 0);
	zassert_true(abs(rl.worst_t_err_us) <= 1200, "the uptime field gives the gap's length");
}

ZTEST(vib_spi, test_line_format)
{
	uint8_t pkt[VIB_MAX_PKT];
	char line[VIB_LINE_MAX];
	int16_t s[VIB_MAX_SAMPLES];
	struct vib_hdr h = {.seq = 0xBEEF, .rate_hz = 6667, .t_ms = 0x01020304,
			    .index = 0xA0B0C0D0, .mode = 11, .flags = 0};

	for (int i = 0; i < VIB_MAX_SAMPLES; i++) {
		s[i] = (int16_t)(i * 1000 - 50000);
	}
	size_t len = vib_encode(&h, s, VIB_MAX_SAMPLES, pkt);
	size_t n = vib_line(pkt, len, line);

	zassert_equal(len, 244);
	zassert_equal(n, 494, "about 58 lines/s, 28.6 kB/s at 6667 Hz");
	zassert_mem_equal(line, "0207 efbe0b1a04030201d0c0b0a00b00", 33);
	for (size_t i = 5; i < n - 1; i++) {
		zassert_true((line[i] >= '0' && line[i] <= '9') || (line[i] >= 'a' && line[i] <= 'f'),
			     "lowercase hex only");
	}
	zassert_equal(line[n - 1], '\n');
}

ZTEST_SUITE(vib_spi, NULL, NULL, NULL, NULL, NULL);
