/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Vibration bench rig (docs/ble_gatt.md 4.5): nRF52 DK, LSM6DSOX on SPIM2 at 8 MHz, printing
 * one Vibration Data packet per line, "0207 <hex>", on the J-Link virtual COM port at 1 Mbaud.
 * After the banner nothing else goes out until button 1 stops the capture and prints one
 * "stats" line. LED 1: streaming. LED 2: the FIFO filled at least once (a gap).
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/atomic.h>

#include <stdio.h>
#include <string.h>

#include "vib_spi.h"

#define IMU_NODE DT_NODELABEL(vib_imu)
#define SPI_OP   (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPOL | \
		  SPI_MODE_CPHA)

static const struct spi_dt_spec imu = SPI_DT_SPEC_GET(IMU_NODE, SPI_OP);
static const struct device *const uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static const struct gpio_dt_spec led_run = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_gap = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec btn_stop = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

/* Line buffers: 16 x 494 B, about 280 ms of output at 58 lines/s. A line that finds no free
 * buffer is dropped; the decoder sees it as a jump in the sample index. */
#define N_LINES 16
static char lines[N_LINES][VIB_LINE_MAX];
static size_t line_len[N_LINES];
K_MSGQ_DEFINE(free_q, sizeof(uint8_t), N_LINES, 1);
K_MSGQ_DEFINE(ready_q, sizeof(uint8_t), N_LINES, 1);
static K_SEM_DEFINE(tx_done, 0, 1);
static uint32_t dropped;
static uint8_t free_min = N_LINES;
static atomic_t stop;

static struct vib_spi vs;

static int xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
	const struct spi_buf tb = {.buf = (void *)tx, .len = len};
	const struct spi_buf rb = {.buf = rx, .len = len};
	const struct spi_buf_set ts = {.buffers = &tb, .count = 1};
	const struct spi_buf_set rs = {.buffers = &rb, .count = 1};

	ARG_UNUSED(ctx);
	return spi_transceive_dt(&imu, &ts, &rs);
}

static uint64_t now_us(void *ctx)
{
	ARG_UNUSED(ctx);
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user);
	if (evt->type == UART_TX_DONE || evt->type == UART_TX_ABORTED) {
		k_sem_give(&tx_done);
	}
}

static void send_text(const char *s, size_t n)
{
	if (uart_tx(uart, (const uint8_t *)s, n, SYS_FOREVER_US) == 0) {
		k_sem_take(&tx_done, K_FOREVER);
	}
}

/* Lower priority than the drain: sends the queued lines one DMA transfer each. */
static void tx_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	for (;;) {
		uint8_t i;

		k_msgq_get(&ready_q, &i, K_FOREVER);
		send_text(lines[i], line_len[i]);
		k_msgq_put(&free_q, &i, K_NO_WAIT);
	}
}

K_THREAD_DEFINE(tx_tid, 1024, tx_thread, NULL, NULL, NULL, 7, 0, 0);

static void emit(const uint8_t *pkt, size_t len, void *user)
{
	uint8_t i;

	ARG_UNUSED(user);
	if (k_msgq_get(&free_q, &i, K_NO_WAIT)) {
		dropped++;
		return;
	}
	line_len[i] = vib_line(pkt, len, lines[i]);
	k_msgq_put(&ready_q, &i, K_NO_WAIT);
	uint8_t f = (uint8_t)k_msgq_num_used_get(&free_q);

	free_min = MIN(free_min, f);
}

static struct gpio_callback btn_cb;

static void on_button(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	atomic_set(&stop, 1);
}

static void say(const char *s)
{
	send_text(s, strlen(s));
}

static void halt(const char *why)
{
	say(why);
	for (;;) {
		gpio_pin_toggle_dt(&led_gap);
		k_msleep(200);
	}
}

int main(void)
{
	static char msg[200];
	uint8_t id = 0;

	for (uint8_t i = 0; i < N_LINES; i++) {
		k_msgq_put(&free_q, &i, K_NO_WAIT);
	}
	gpio_pin_configure_dt(&led_run, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_gap, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&btn_stop, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&btn_stop, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&btn_cb, on_button, BIT(btn_stop.pin));
	gpio_add_callback(btn_stop.port, &btn_cb);
	uart_callback_set(uart, uart_cb, NULL);

	vs.xfer = xfer;
	vs.now_us = now_us;
	k_msleep(20); /* the chip's boot time, 10 ms [DS12814] */
	if (!spi_is_ready_dt(&imu)) {
		halt("vib_bench: SPI not ready\n");
	}
	(void)vib_spi_write(&vs, VIB_SPI_CTRL3_C, VIB_SPI_CTRL3_RESET);
	k_msleep(5);
	(void)vib_spi_read(&vs, VIB_SPI_WHO_AM_I, &id, 1);
	if (id != VIB_SPI_WHO_AM_I_ID) {
		snprintf(msg, sizeof(msg),
			 "vib_bench: WHO_AM_I 0x%02x, expected 0x6c: check wiring and power\n", id);
		halt(msg);
	}
	snprintf(msg, sizeof(msg),
		 "vib_bench: LSM6DSOX on SPI %u Hz, 6667 Hz, +-2 g, chip +Z, mode 11 packets "
		 "follow as '0207 <hex>'; button 1 stops\n",
		 (unsigned int)imu.config.frequency);
	say(msg);
	if (vib_spi_start(&vs)) {
		halt("vib_bench: setup writes failed\n");
	}
	gpio_pin_set_dt(&led_run, 1);

	while (!atomic_get(&stop)) {
		int n = vib_spi_poll(&vs, emit, NULL);

		if (vs.st.gaps) {
			gpio_pin_set_dt(&led_gap, 1);
		}
		if (n <= 0) {
			k_usleep(1000); /* under 7 new words; the watermark is 36 (5.4 ms) */
		}
	}

	/* stop: accelerometer off, FIFO to bypass, let the queued lines out, then the stats */
	struct vib_reg w[VIB_MAX_WRITES];
	size_t k = vib_transition(MODE_RESEARCH_VIB6K7, MODE_FULL, w);

	for (size_t i = 0; i < k; i++) {
		(void)vib_spi_write(&vs, w[i].reg, w[i].val);
	}
	gpio_pin_set_dt(&led_run, 0);
	while (k_msgq_num_used_get(&free_q) < N_LINES) {
		k_msleep(10);
	}
	snprintf(msg, sizeof(msg),
		 "stats samples %u bursts %u polls %u max_fifo_words %u gaps %u spi_errors %u "
		 "lines_dropped %u min_free_lines %u\n",
		 (unsigned int)vs.st.words, (unsigned int)vs.st.bursts,
		 (unsigned int)vs.st.polls, (unsigned int)vs.st.max_level,
		 (unsigned int)vs.st.gaps, (unsigned int)vs.st.errors, (unsigned int)dropped,
		 (unsigned int)free_min);
	say(msg);
	return 0;
}
