/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * The case LED language (docs/charging-case.md, "LED and button language") as colour
 * over time. Plain C: main.c turns the colour into TIM2 PWM. Colours and timings that the
 * spec leaves open are [ASSUMED], to tune behind the light pipe.
 */
#ifndef CASE_LED_H_
#define CASE_LED_H_

#include <stdbool.h>
#include <stdint.h>

enum led_pat {
	LED_OFF = 0,
	LED_POD_CHARGING, /* amber, slow breathe, 5 s */
	LED_POD_FULL,     /* green steady, 5 s */
	LED_BATT_HIGH,    /* case battery > 60 %: 3 green blinks */
	LED_BATT_MID,     /* 20-60 %: 2 amber blinks */
	LED_BATT_LOW,     /* < 20 %: 1 red blink */
	LED_FAULT,        /* pod contact over-current: red fast blink, 5 s */
	LED_LOWBAT,       /* case cell below the cut: red blinks */
	LED_SOFT_RESET,   /* white flash */
	LED_HARD_RESET,   /* white double flash */
	LED_SWD_FAIL,     /* the hard reset did not reach the pod: 2 slow red blinks [ASSUMED] */
	LED_USB_CHARGING, /* amber steady while USB is in and the case cell charges (no end) */
	LED_USB_FULL,     /* green steady while USB is in and the cell is full (no end) */
	LED_PAT_COUNT,
};

struct rgb {
	uint8_t r, g, b;
};

/* The colour `t` ms into pattern `p`. False once the pattern has ended (colour off). */
bool led_render(enum led_pat p, uint32_t t, struct rgb *c);

/* The pattern's length in ms, 0 for one without an end. */
uint32_t led_length(enum led_pat p);

/* The battery pattern for a charge in %. */
enum led_pat led_batt(uint8_t pct);

#endif /* CASE_LED_H_ */
