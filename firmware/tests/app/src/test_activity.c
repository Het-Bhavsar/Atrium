/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits.h>
#include <zephyr/ztest.h>

#include "activity.h"

ZTEST_SUITE(activity, NULL, NULL, NULL, NULL, NULL);

ZTEST(activity, test_still_and_moving)
{
	struct activity a;
	struct activity_minute m;

	activity_init(&a);
	for (int min = 0; min < ACTIVITY_STILL_MIN; min++) {
		for (int i = 0; i < 60 * 15; i++) {
			activity_push(&a, 0, 0, 1000 + (i % 2 ? 5 : -5));
		}
		m = activity_close(&a);
		zassert_equal(m.index_mg, 5);
		zassert_false(m.moving);
	}
	zassert_true(m.still);

	for (int i = 0; i < 60 * 15; i++) {
		activity_push(&a, 300, 0, (i % 2) ? 1400 : 700);
	}
	m = activity_close(&a);
	zassert_true(m.moving, "index %u", m.index_mg);
	zassert_false(m.still);
}

ZTEST(activity, test_posture_angles)
{
	int16_t p, r;

	/* gravity along +z: level */
	posture_angles(0, 0, 1000 * 10, 10, &p, &r);
	zassert_equal(p, 0);
	zassert_equal(r, 0);
	/* along +y: rolled 90 degrees */
	posture_angles(0, 1000 * 10, 0, 10, &p, &r);
	zassert_equal(r, 90);
	/* along -x: pitched up 90 degrees */
	posture_angles(-1000 * 10, 0, 0, 10, &p, &r);
	zassert_equal(p, 90);
	/* upside down */
	posture_angles(0, 0, -1000, 1, &p, &r);
	zassert_true(r == 180 || r == -180);
	posture_angles(0, 0, 0, 0, &p, &r);
	zassert_equal(p, INT16_MIN);
}

ZTEST(activity, test_empty_minute)
{
	struct activity a;

	activity_init(&a);
	struct activity_minute m = activity_close(&a);

	zassert_equal(m.index_mg, 0);
	zassert_false(m.still);
}
