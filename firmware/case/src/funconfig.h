/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * ch32fun configuration for the case MCU.
 */
#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

#define FUNCONF_USE_HSI 1
#define FUNCONF_USE_PLL 0 /* 24 MHz HSI: enough for 10 Hz work and SWD, and half the run current */
#define FUNCONF_SYSTEM_CORE_CLOCK 24000000
#define FUNCONF_SYSTICK_USE_HCLK 0 /* SysTick at HCLK / 8 = 3 MHz */
#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_USE_UARTPRINTF 0

#endif
