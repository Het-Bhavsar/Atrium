/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Activity index: mean of the absolute value of |a| - 1 g over a minute, in mg
 * (docs/ble_gatt.md section 6). Plain C, no Zephyr.
 */
#ifndef ATRIUM_ACTIVITY_H_
#define ATRIUM_ACTIVITY_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ACTIVITY_MOVING_MG 50 /* [ASSUMED] head-worn walking is well above this */
#define ACTIVITY_STILL_MG  15 /* [ASSUMED] */
#define ACTIVITY_STILL_MIN 10

struct activity {
	uint64_t sum_mg;
	uint32_t n;
	uint16_t still_min;
};

/* A chip-to-pod axis rotation: 9 entries, row-major, pod = M x chip. Valid when it is a
 * proper rotation of axes: one +-1 in each row and column, determinant +1. */
bool axes_valid(const int8_t m[9]);
void axes_to_pod(const int8_t m[9], const int32_t chip[3], int32_t pod[3]);

/* Head angle from the mean gravity vector, pod frame (atrium_ml position.py): +x up the long
 * axis to the antenna end, +z out of the top face, +y = z x x. The pod is worn straight
 * upright (no tilt), so upright gives pitch -90 with roll undefined; lying puts gravity in the
 * y-z plane, where roll resolves the side. A placement error moves each angle by no more
 * than the error.
 * pitch = atan2(-x, sqrt(y^2 + z^2)) in degrees, roll = atan2(y, z) in degrees.
 * Both INT16_MIN when there are no samples. */
void posture_angles(int64_t sx, int64_t sy, int64_t sz, uint32_t n, int16_t *pitch_deg,
		    int16_t *roll_deg);

struct activity_minute {
	uint16_t index_mg;
	bool moving;
	bool still; /* still for ACTIVITY_STILL_MIN minutes or more */
};

void activity_init(struct activity *a);
void activity_push(struct activity *a, int32_t x_mg, int32_t y_mg, int32_t z_mg);
/* Close the minute and start the next. */
struct activity_minute activity_close(struct activity *a);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_ACTIVITY_H_ */
