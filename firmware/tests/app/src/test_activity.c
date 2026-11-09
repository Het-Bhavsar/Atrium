/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits.h>
#include <math.h>
#include <stdlib.h>
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

ZTEST(activity, test_axes_rotation)
{
	/* the EVT placement (atrium_pod.dts): chip +Y = pod +x, chip +X = pod -y */
	static const int8_t evt[9] = {0, 1, 0, -1, 0, 0, 0, 0, 1};
	static const int8_t ident[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
	static const int8_t mirror[9] = {-1, 0, 0, 0, 1, 0, 0, 0, 1}; /* det -1 */
	static const int8_t twice[9] = {1, 1, 0, 0, 0, 0, 0, 0, 1};
	int32_t pod[3];

	zassert_true(axes_valid(evt));
	zassert_true(axes_valid(ident));
	zassert_false(axes_valid(mirror), "a mirror is not a rotation");
	zassert_false(axes_valid(twice));
	/* worn upright, the antenna end is up: the chip reads +1 g on its Y */
	axes_to_pod(evt, (const int32_t[3]){0, 1000, 0}, pod);
	zassert_equal(pod[0], 1000);
	zassert_equal(pod[1], 0);
	axes_to_pod(evt, (const int32_t[3]){1000, 0, 0}, pod);
	zassert_equal(pod[1], -1000, "chip +X along pod -y");
}

/* Straight upright wear (2026-11-07: long axis vertical, no tilt toward the eye), pod
 * frame, right temporal region: the cases of atrium_ml position.POD_UP through the log's
 * 1-degree steps */
ZTEST(activity, test_vertical_wear_positions)
{
	int16_t p, r;

	/* upright: +x straight up, pitch -90, roll is noise */
	posture_angles(1000, 0, 0, 1, &p, &r);
	zassert_equal(p, -90);
	/* on the back: gravity on -y */
	posture_angles(0, -1000, 0, 1, &p, &r);
	zassert_equal(p, 0);
	zassert_equal(r, -90);
	/* face down: +y */
	posture_angles(0, 1000, 0, 1, &p, &r);
	zassert_equal(r, 90);
	/* on the pod's side, pod against the pillow: -z */
	posture_angles(0, 0, -1000, 1, &p, &r);
	zassert_equal(p, 0);
	zassert_true(r == 180 || r == -180);
	/* on the other side: +z */
	posture_angles(0, 0, 1000, 1, &p, &r);
	zassert_equal(r, 0);
}

/* ML's expected log angles (pitch, roll) on both sides of the head. On the left, +y points
 * forward, so the roll signs flip; the app knows the side */
static const struct {
	const char *pos;
	double right[3], left[3]; /* gravity, g, pod frame */
	int16_t pitch, roll_right, roll_left;
	bool roll_defined;
} wear[] = {
	{"upright", {1, 0, 0}, {1, 0, 0}, -90, 0, 0, false},
	{"back", {0, -1, 0}, {0, 1, 0}, 0, -90, 90, true},
	{"face down", {0, 1, 0}, {0, -1, 0}, 0, 90, -90, true},
	{"pod side", {0, 0, -1}, {0, 0, -1}, 0, 180, 180, true},
	{"other side", {0, 0, 1}, {0, 0, 1}, 0, 0, 0, true},
};

static int angle_diff(int a, int b)
{
	int d = (a - b) % 360;

	return d > 180 ? d - 360 : (d < -180 ? d + 360 : d);
}

static void check(const char *pos, const char *side, const double g[3], int16_t pitch,
		  int16_t roll, bool roll_defined, int tol)
{
	int16_t p, r;

	posture_angles(lround(g[0] * 1000), lround(g[1] * 1000), lround(g[2] * 1000), 1, &p, &r);
	zassert_within(p, pitch, tol, "%s %s pitch %d", pos, side, p);
	if (roll_defined) {
		zassert_true(abs(angle_diff(r, roll)) <= tol, "%s %s roll %d", pos, side, r);
	}
}

ZTEST(activity, test_vertical_wear_both_sides)
{
	for (size_t i = 0; i < ARRAY_SIZE(wear); i++) {
		check(wear[i].pos, "right", wear[i].right, wear[i].pitch, wear[i].roll_right,
		      wear[i].roll_defined, 0);
		check(wear[i].pos, "left", wear[i].left, wear[i].pitch, wear[i].roll_left,
		      wear[i].roll_defined, 0);
	}
}

/* People won't place it exactly: up to 15 degrees turned in the skin plane (about z) or rocked
 * (about y) moves each angle by no more than the error, so the app's +-15 degree classes hold */
ZTEST(activity, test_vertical_wear_placement_error)
{
	for (int deg = -15; deg <= 15; deg += 5) {
		double a = deg * 3.14159265358979 / 180.0, c = cos(a), sn = sin(a);

		for (size_t i = 0; i < ARRAY_SIZE(wear); i++) {
			for (int side = 0; side < 2; side++) {
				const double *g = side ? wear[i].left : wear[i].right;
				int16_t roll = side ? wear[i].roll_left : wear[i].roll_right;
				double gz[3] = {g[0] * c - g[1] * sn, g[0] * sn + g[1] * c, g[2]};
				double gy[3] = {g[0] * c + g[2] * sn, g[1], -g[0] * sn + g[2] * c};
				const char *s = side ? "left" : "right";

				check(wear[i].pos, s, gz, wear[i].pitch, roll, wear[i].roll_defined,
				      abs(deg) + 1);
				check(wear[i].pos, s, gy, wear[i].pitch, roll, wear[i].roll_defined,
				      abs(deg) + 1);
			}
		}
	}
}

ZTEST(activity, test_empty_minute)
{
	struct activity a;

	activity_init(&a);
	struct activity_minute m = activity_close(&a);

	zassert_equal(m.index_mg, 0);
	zassert_false(m.still);
}
