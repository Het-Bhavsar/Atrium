/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAXM86161 optical module driver (I2C 0x62). LDO_EN gates the module's
 * supply: 0.05 uA off, 1.6 uA in SHDN (data sheet ILEDSHDN), so the driver
 * powers it down fully when no slots run.
 */
#ifndef ATRIUM_DRIVERS_MAXM86161_H_
#define ATRIUM_DRIVERS_MAXM86161_H_

#include <zephyr/device.h>

#include "maxm86161_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Called from the system work queue with raw 3-byte FIFO items (MSB first). */
typedef void (*maxm86161_fifo_cb_t)(const struct device *dev, const uint8_t *items, size_t n,
				    void *user_data);

/* Power the module, reset it, load the slots at one rate, and start sampling. */
int maxm86161_start(const struct device *dev, const struct maxm86161_slot *slots, size_t n,
		    uint16_t hz, maxm86161_fifo_cb_t cb, void *user_data);

/* Stop and cut the module's supply (LDO_EN low). */
int maxm86161_off(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_DRIVERS_MAXM86161_H_ */
