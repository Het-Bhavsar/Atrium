/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * ppg.h backend for the MAXM86161A (pod U3; the MAXM86161 has the same register map).
 */
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include "maxm86161.h"
#include "ppg.h"

static const struct device *const afe = DEVICE_DT_GET(DT_ALIAS(ppg0));
static ppg_sample_cb_t sample_cb;
static ppg_raw_cb_t raw_cb;

static void fifo_data(const struct device *dev, const uint8_t *items, size_t n, void *user)
{
	ppg_sample_cb_t s = sample_cb;
	ppg_raw_cb_t r = raw_cb;

	ARG_UNUSED(dev);
	ARG_UNUSED(user);
	for (size_t i = 0; s && i < n; i++) {
		uint8_t tag;
		uint32_t v;

		maxm86161_fifo_decode(&items[3 * i], &tag, &v);
		int slot = maxm86161_tag_slot(tag);

		if (slot >= 0) {
			s((uint8_t)slot, 0, (int32_t)v); /* one photodiode: channel 0 */
		}
	}
	if (r) {
		r(items, n * 3);
	}
}

bool ppg_ready(void)
{
	return device_is_ready(afe);
}

int ppg_apply(const struct mode_cfg *m)
{
	struct maxm86161_slot slots[MODE_MAX_SLOTS];

	if (!device_is_ready(afe)) {
		return -ENODEV;
	}
	(void)maxm86161_off(afe);
	if (m->n_slots == 0) {
		return 0;
	}
	for (uint8_t i = 0; i < m->n_slots; i++) {
		/* LEDCn codes: green 1, IR 2, red 3 (enum led_id + 1), direct ambient 9 */
		slots[i].ledc = m->slot[i].led == LED_AMBIENT ? MAXM86161_LEDC_DA
							      : (uint8_t)(m->slot[i].led + 1U);
		slots[i].led_ua = m->slot[i].ma * 1000U;
	}
	return maxm86161_start(afe, slots, m->n_slots, mode_sample_hz(m), mode_avg(m), fifo_data,
			       NULL);
}

void ppg_set_callbacks(ppg_sample_cb_t sample, ppg_raw_cb_t raw)
{
	sample_cb = sample;
	raw_cb = raw;
}

size_t ppg_raw_word_bytes(void)
{
	return 3;
}
