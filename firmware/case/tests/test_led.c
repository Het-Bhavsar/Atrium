/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "check.h"
#include "led.h"

static bool lit(struct rgb c)
{
	return c.r || c.g || c.b;
}

/* lit stretches over the pattern, sampled every 10 ms */
static int flashes(enum led_pat p, struct rgb *colour)
{
	struct rgb c;
	bool was = false;
	int n = 0;

	for (uint32_t t = 0; led_render(p, t, &c); t += 10) {
		if (lit(c) && !was) {
			n++;
			*colour = c;
		}
		was = lit(c);
	}
	return n;
}

static void lengths_and_ends(void)
{
	struct rgb c;

	for (int p = LED_POD_CHARGING; p < LED_PAT_COUNT; p++) {
		uint32_t len = led_length((enum led_pat)p);

		if (p == LED_USB_CHARGING || p == LED_USB_FULL) {
			CHECK_EQ(len, 0);
			CHECK(led_render((enum led_pat)p, 3600000, &c) && lit(c));
			continue;
		}
		CHECK(len > 0);
		CHECK(led_render((enum led_pat)p, len - 1, &c));
		CHECK(!led_render((enum led_pat)p, len, &c));
		CHECK(!lit(c));
	}
	CHECK(!led_render(LED_OFF, 0, &c));
	CHECK_EQ(led_length(LED_POD_CHARGING), 5000);
	CHECK_EQ(led_length(LED_POD_FULL), 5000);
}

static void blink_counts_and_colours(void)
{
	struct rgb c = {0};

	CHECK_EQ(flashes(LED_BATT_HIGH, &c), 3);
	CHECK(c.g && !c.r && !c.b);
	CHECK_EQ(flashes(LED_BATT_MID, &c), 2);
	CHECK(c.r && c.g && !c.b && c.r > c.g); /* amber */
	CHECK_EQ(flashes(LED_BATT_LOW, &c), 1);
	CHECK(c.r && !c.g && !c.b);
	CHECK_EQ(flashes(LED_SOFT_RESET, &c), 1);
	CHECK(c.r && c.g && c.b);
	CHECK_EQ(flashes(LED_HARD_RESET, &c), 2);
	CHECK(c.r && c.g && c.b);
	CHECK_EQ(flashes(LED_FAULT, &c), 25); /* 5 Hz for 5 s */
	CHECK(c.r && !c.g);
	CHECK_EQ(flashes(LED_POD_FULL, &c), 1);
	CHECK(c.g && !c.r);
}

static void breathe(void)
{
	struct rgb c;

	led_render(LED_POD_CHARGING, 0, &c);
	CHECK_EQ(c.r, 0);
	led_render(LED_POD_CHARGING, 1250, &c);
	CHECK(c.r > 250 && c.g > 0 && c.b == 0);
	led_render(LED_POD_CHARGING, 625, &c);
	CHECK(c.r > 40 && c.r < 90); /* squared: a quarter way in is about a quarter bright */
	led_render(LED_POD_CHARGING, 2500, &c);
	CHECK_EQ(c.r, 0);
	led_render(LED_POD_CHARGING, 3750, &c);
	CHECK(c.r > 250); /* the second breath */
}

static void battery_bands(void)
{
	CHECK_EQ(led_batt(100), LED_BATT_HIGH);
	CHECK_EQ(led_batt(61), LED_BATT_HIGH);
	CHECK_EQ(led_batt(60), LED_BATT_MID);
	CHECK_EQ(led_batt(20), LED_BATT_MID);
	CHECK_EQ(led_batt(19), LED_BATT_LOW);
	CHECK_EQ(led_batt(0), LED_BATT_LOW);
}

const struct check_case led_cases[] = {
	{"led: lengths and ends", lengths_and_ends},
	{"led: blink counts and colours", blink_counts_and_colours},
	{"led: amber breathe", breathe},
	{"led: battery bands", battery_bands},
	CASES_END,
};
