/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * BQ21061 on I2C and its /PG pin. /LP is tied low, so the charger answers only while
 * VIN is present: every call except charger_pg_low() fails off the Plinth.
 */
#ifndef ATRIUM_CHARGER_H_
#define ATRIUM_CHARGER_H_

#include <stdbool.h>
#include <stdint.h>

#include "dock.h"

int charger_init(void);

/* /PG low: a valid input is present. */
bool charger_pg_low(void);

/* /PG went low (VIN good) since the last call: the BQ21061 may have reset its registers, so
 * the caller services it again. Edges also feed the case patterns (vinpat.h): three VIN
 * pulses within 2 s reboot the pod from the interrupt. */
bool charger_vin_good_edge(void);

/* Fed by the charging case now (its VIN signature seen since VIN came on). */
bool charger_on_case(void);

/*
 * Service the charger, at least every 20 s while docked: the I2C watchdog (50 s)
 * returns every register to reset when the host goes quiet. Checks the device ID once,
 * writes ICHG (cold or full current) and the TS thresholds, sets CHARGER_DISABLE and
 * reads the status. stat0/stat1 get the raw status registers (0 on failure).
 */
int charger_service(bool cold, bool disable, struct chg_report *out, uint8_t *stat0,
		    uint8_t *stat1);

/* Ship mode at the next lift: the BQ21061 waits for VIN to go away. Only on the Plinth. */
int charger_ship_on_lift(void);

#endif /* ATRIUM_CHARGER_H_ */
