/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "activity.h"

#include <math.h>
#include <string.h>

void posture_angles(int64_t sx, int64_t sy, int64_t sz, uint32_t n, int16_t *pitch_deg,
		    int16_t *roll_deg)
{
	if (n == 0) {
		*pitch_deg = INT16_MIN;
		*roll_deg = INT16_MIN;
		return;
	}
	float x = (float)sx / n, y = (float)sy / n, z = (float)sz / n;
	const float deg = 57.2957795f;

	*pitch_deg = (int16_t)lroundf(atan2f(-x, sqrtf(y * y + z * z)) * deg);
	*roll_deg = (int16_t)lroundf(atan2f(y, z) * deg);
}

void activity_init(struct activity *a)
{
	memset(a, 0, sizeof(*a));
}

void activity_push(struct activity *a, int32_t x_mg, int32_t y_mg, int32_t z_mg)
{
	float m = sqrtf((float)x_mg * x_mg + (float)y_mg * y_mg + (float)z_mg * z_mg);

	a->sum_mg += (uint64_t)lroundf(fabsf(m - 1000.0f));
	a->n++;
}

struct activity_minute activity_close(struct activity *a)
{
	struct activity_minute out = {0};

	if (a->n) {
		uint64_t idx = a->sum_mg / a->n;

		out.index_mg = idx > UINT16_MAX ? UINT16_MAX : (uint16_t)idx;
	}
	out.moving = out.index_mg >= ACTIVITY_MOVING_MG;
	if (a->n && out.index_mg < ACTIVITY_STILL_MG) {
		if (a->still_min < UINT16_MAX) {
			a->still_min++;
		}
	} else {
		a->still_min = 0;
	}
	out.still = a->still_min >= ACTIVITY_STILL_MIN;
	a->sum_mg = 0;
	a->n = 0;
	return out;
}
