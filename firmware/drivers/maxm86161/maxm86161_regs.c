/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "maxm86161_regs.h"

#include <string.h>

int maxm86161_led_code(uint32_t led_ua, uint8_t *range, uint8_t *code)
{
	for (uint8_t rge = 0; rge < 4; rge++) {
		uint32_t fs_ua = 31000U * (rge + 1U);
		uint32_t c = (led_ua * 255U + fs_ua / 2U) / fs_ua;

		if (led_ua <= fs_ua && c <= 0xFF) {
			*range = rge;
			*code = (uint8_t)c;
			return 0;
		}
	}
	return -1;
}

uint32_t maxm86161_led_ua(uint8_t range, uint8_t code)
{
	return (31000U * (range + 1U) * code + 127U) / 255U;
}

int maxm86161_sr_code(uint16_t hz)
{
	/* PPG_SR table, 32768 Hz clock, one pulse per sample */
	static const struct {
		uint16_t hz;
		uint8_t code;
	} t[] = {{8, 0x0A},  {16, 0x0B}, {25, 0x00}, {32, 0x0C},  {50, 0x01}, {64, 0x0D},
		 {84, 0x02}, {100, 0x03}, {128, 0x0E}, {200, 0x04}, {256, 0x0F}};

	for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
		if (t[i].hz == hz) {
			return t[i].code;
		}
	}
	return -1;
}

int maxm86161_encode(const struct maxm86161_slot *s, size_t n, uint16_t hz,
		     struct maxm86161_cfg_regs *r)
{
	return maxm86161_encode_avg(s, n, hz, 1, r);
}

int maxm86161_encode_avg(const struct maxm86161_slot *s, size_t n, uint16_t hz, uint8_t avg,
			 struct maxm86161_cfg_regs *r)
{
	int sr = maxm86161_sr_code(hz);
	uint16_t used = 0;
	uint8_t ave = 0; /* SMP_AVE: log2(avg) */

	memset(r, 0, sizeof(*r));
	while (ave < 7 && (1U << ave) < avg) {
		ave++;
	}
	if (sr < 0 || n == 0 || n > MAXM86161_N_SLOTS || avg == 0 || (1U << ave) != avg) {
		return -1;
	}
	for (size_t i = 0; i < n; i++) {
		uint8_t led = s[i].ledc;
		uint8_t range, code;
		bool da = led == MAXM86161_LEDC_DA;

		if (!da && (led < MAXM86161_LEDC_GREEN || led > MAXM86161_LEDC_RED)) {
			return -1;
		}
		if ((used & (1U << led)) || (!da && maxm86161_led_code(s[i].led_ua, &range, &code))) {
			return -1;
		}
		used |= (uint16_t)(1U << led);
		r->seq[i / 2] |= (uint8_t)(led << ((i % 2) * 4));
		if (!da) {
			r->pa[led - 1] = code;
			r->range1 |= (uint8_t)(range << ((led - 1) * 2));
		}
	}
	r->ppg_cfg2 = (uint8_t)((sr << 3) | ave);
	return 0;
}

int maxm86161_tag_slot(uint8_t tag)
{
	if (tag >= MAXM86161_TAG_LEDC1 && tag < MAXM86161_TAG_LEDC1 + MAXM86161_N_SLOTS) {
		return tag - MAXM86161_TAG_LEDC1;
	}
	if (tag >= MAXM86161_TAG_PF_LEDC1 && tag < MAXM86161_TAG_PF_LEDC1 + 3) {
		return tag - MAXM86161_TAG_PF_LEDC1;
	}
	return -1;
}
