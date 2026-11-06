/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "status_led.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define ON_MS  20
#define GAP_MS 180

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static struct k_work_delayable work;
static atomic_t left; /* half-steps: odd = on phase */

static void step(struct k_work *w)
{
	ARG_UNUSED(w);
	atomic_val_t n = atomic_get(&left);

	if (n <= 0) {
		(void)gpio_pin_set_dt(&led, 0);
		return;
	}
	bool on = n % 2 == 0;

	(void)gpio_pin_set_dt(&led, on ? 1 : 0);
	atomic_dec(&left);
	(void)k_work_reschedule(&work, K_MSEC(on ? ON_MS : GAP_MS));
}

int status_led_init(void)
{
	k_work_init_delayable(&work, step);
	if (!gpio_is_ready_dt(&led)) {
		return -ENODEV;
	}
	return gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
}

void status_led_flash(uint8_t n)
{
	atomic_set(&left, 2 * n);
	(void)k_work_reschedule(&work, K_NO_WAIT);
}
