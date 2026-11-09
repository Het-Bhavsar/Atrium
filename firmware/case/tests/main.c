/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Runs every case host test. Usage: make -C firmware/case test
 */
#include <stdio.h>

#include "check.h"

int check_fails;
const char *check_test;

extern const struct check_case sense_cases[], led_cases[], swd_cases[], sm_cases[];

static int run(const struct check_case *c)
{
	int n = 0;

	for (; c->name; c++, n++) {
		int before = check_fails;

		check_test = c->name;
		c->fn();
		printf("%s %s\n", check_fails == before ? "ok  " : "FAIL", c->name);
	}
	return n;
}

int main(void)
{
	int n = run(sense_cases) + run(led_cases) + run(swd_cases) + run(sm_cases);

	printf("%d tests, %d failed checks\n", n, check_fails);
	return check_fails ? 1 : 0;
}
