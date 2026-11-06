/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Status LED under the light pipe (D2 on P0.17, about 1 mA). Short flashes only:
 * one 20 ms flash every 4 s is 5 uA.
 */
#ifndef ATRIUM_STATUS_LED_H_
#define ATRIUM_STATUS_LED_H_

#include <stdint.h>

int status_led_init(void);

/* n flashes of 20 ms, 200 ms apart. A new request replaces one in progress. */
void status_led_flash(uint8_t n);

#endif /* ATRIUM_STATUS_LED_H_ */
