/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * The CH32V003 independent watchdog (IWDG) settings, in plain C so the host tests pin the
 * arithmetic. Why it is there: in a pogo short the SY6280 holds about 0.45 A through D1/D2,
 * and only the firmware's 80 mA cut keeps the diode inside its rating. A hung MCU with VIN_EN
 * high would cook it. With the watchdog, a hang resets the MCU; at reset PD0 floats and R10
 * (1 MOhm) pulls the SY6280 EN low, so the feed stops.
 *
 *  - Production sets the user option byte to WDT_OB_USER (0xDE): IWDG_SW = 0, so the watchdog
 *    runs from reset in hardware mode and software cannot stop it (firmware/case/README.md,
 *    "Flashing"). main.c also starts it first thing, so a chip still in software mode is
 *    covered from the first lines of main, before VIN_EN can go high.
 *  - Awake: 100 ms (WDT_RUN_*). main feeds it once per 20 ms LED frame, after that frame's
 *    over-current check, and nowhere else: never from an interrupt.
 *  - Standby: the watchdog keeps counting on the LSI [ASSUMED: CH32V003RM gives no way to
 *    freeze it], and standby can last 30 s. So with VIN off, just before standby, main stretches
 *    it to 8.19 s (WDT_SLEEP_*) and caps the AWU wake-up at 7.2 s (WDT_AWU_MAX_COUNTS). Both run
 *    on the same LSI, so the 12 % margin holds whatever the LSI's error. On waking it goes back
 *    to 100 ms before anything else.
 *  - The debug unit freezes it while a programmer halts the core (DBGMCU_CR IWDG_STOP, set by
 *    main), so flashing a running unit is not cut short [VERIFY on the bench].
 * LSI 128 kHz nominal [VERIFY its tolerance in the CH32V003 data sheet]; at the LSI's limits the
 * 100 ms timeout moves in proportion.
 */
#ifndef CASE_WDT_H_
#define CASE_WDT_H_

#include <stdint.h>

#define WDT_LSI_HZ 128000

/* IWDG_PSCR codes: divider 4 << code (4 .. 256) */
#define WDT_PR_4   0
#define WDT_PR_256 6

/* awake: (3199 + 1) x 4 / 128 kHz = 100 ms */
#define WDT_RUN_PR WDT_PR_4
#define WDT_RUN_RL 3199
/* standby: (4095 + 1) x 256 / 128 kHz = 8.192 s, the longest the IWDG has */
#define WDT_SLEEP_PR WDT_PR_256
#define WDT_SLEEP_RL 4095

/* AWU: 61440 / 128 kHz = 480 ms a count; 15 counts = 7.2 s, under WDT_SLEEP */
#define WDT_AWU_STEP_US    480000
#define WDT_AWU_MAX_COUNTS 15

/* User option byte for production: IWDG hardware (bit 0 = 0), no reset on STOP (bit 1) or
 * standby (bit 2), PD7 as GPIO, not NRST (bits 4:3 = 11, the blue LED), start in user flash
 * (bit 5 = 0), bits 7:6 = 11 (ch32fun ch32v003hw.h OB_* values). Its complement is 0x21. */
#define WDT_OB_USER 0xDE

/* The timeout of a prescaler code and reload value, in microseconds. */
static inline uint32_t wdt_timeout_us(uint8_t pr, uint16_t rl)
{
	return (uint32_t)(((uint64_t)(rl + 1U) * (4U << pr) * 1000000U) / WDT_LSI_HZ);
}

#endif /* CASE_WDT_H_ */
