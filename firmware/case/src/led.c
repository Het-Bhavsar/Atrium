/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "led.h"

static const struct rgb OFF = {0, 0, 0};
static const struct rgb RED = {255, 0, 0};
static const struct rgb GREEN = {0, 255, 0};
static const struct rgb AMBER = {255, 90, 0}; /* [ASSUMED] tune behind the light pipe */
static const struct rgb WHITE = {255, 255, 255};

#define BREATHE_MS 2500 /* one breath; two in the 5 s */
#define STEADY_MS  5000
#define BLINK_ON   200
#define BLINK_OFF  300

uint32_t led_length(enum led_pat p)
{
	switch (p) {
	case LED_POD_CHARGING:
	case LED_POD_FULL:
	case LED_FAULT:
		return STEADY_MS;
	case LED_BATT_HIGH:
		return 3 * (BLINK_ON + BLINK_OFF);
	case LED_BATT_MID:
		return 2 * (BLINK_ON + BLINK_OFF);
	case LED_BATT_LOW:
		return 1 * (BLINK_ON + BLINK_OFF);
	case LED_LOWBAT:
		return 3 * 500;
	case LED_SOFT_RESET:
		return 300;
	case LED_HARD_RESET:
		return 600;
	case LED_SWD_FAIL:
		return 2 * 1000;
	default:
		return 0; /* off, and the USB patterns, which have no end */
	}
}

static struct rgb scale(struct rgb c, uint8_t k)
{
	return (struct rgb){(uint8_t)(c.r * k / 255), (uint8_t)(c.g * k / 255),
			    (uint8_t)(c.b * k / 255)};
}

/* on for `on` ms of every `period` */
static bool blink(uint32_t t, uint32_t period, uint32_t on)
{
	return t % period < on;
}

bool led_render(enum led_pat p, uint32_t t, struct rgb *c)
{
	uint32_t len = led_length(p);

	*c = OFF;
	if (p == LED_OFF || p >= LED_PAT_COUNT || (len && t >= len)) {
		return false;
	}
	switch (p) {
	case LED_POD_CHARGING: {
		/* triangle up and down, squared so it looks even to the eye */
		uint32_t ph = t % BREATHE_MS;
		uint32_t tri = ph < BREATHE_MS / 2 ? ph * 510 / BREATHE_MS
						   : (BREATHE_MS - ph) * 510 / BREATHE_MS;
		uint8_t k = (uint8_t)(tri * tri / 255);

		*c = scale(AMBER, k);
		break;
	}
	case LED_POD_FULL:
	case LED_USB_FULL:
		*c = GREEN;
		break;
	case LED_USB_CHARGING:
		*c = AMBER;
		break;
	case LED_BATT_HIGH:
		*c = blink(t, BLINK_ON + BLINK_OFF, BLINK_ON) ? GREEN : OFF;
		break;
	case LED_BATT_MID:
		*c = blink(t, BLINK_ON + BLINK_OFF, BLINK_ON) ? AMBER : OFF;
		break;
	case LED_BATT_LOW:
		*c = blink(t, BLINK_ON + BLINK_OFF, BLINK_ON) ? RED : OFF;
		break;
	case LED_FAULT:
		*c = blink(t, 200, 100) ? RED : OFF;
		break;
	case LED_LOWBAT:
		*c = blink(t, 500, 250) ? RED : OFF;
		break;
	case LED_SOFT_RESET:
		*c = t < 150 ? WHITE : OFF;
		break;
	case LED_HARD_RESET:
		*c = (t < 150 || (t >= 300 && t < 450)) ? WHITE : OFF;
		break;
	case LED_SWD_FAIL:
		*c = blink(t, 1000, 500) ? RED : OFF;
		break;
	default:
		break;
	}
	return true;
}

enum led_pat led_batt(uint8_t pct)
{
	return pct > 60 ? LED_BATT_HIGH : pct >= 20 ? LED_BATT_MID : LED_BATT_LOW;
}
