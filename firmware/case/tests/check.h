/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * A small test runner for the case host tests: no dependencies beyond the C library.
 */
#ifndef CASE_CHECK_H_
#define CASE_CHECK_H_

#include <stdio.h>

extern int check_fails;
extern const char *check_test;

#define CHECK(cond)                                                                          \
	do {                                                                                 \
		if (!(cond)) {                                                               \
			check_fails++;                                                       \
			printf("FAIL %s: %s:%d: %s\n", check_test, __FILE__, __LINE__, #cond); \
		}                                                                            \
	} while (0)

#define CHECK_EQ(a, b)                                                                       \
	do {                                                                                 \
		long long a_ = (long long)(a), b_ = (long long)(b);                         \
		if (a_ != b_) {                                                              \
			check_fails++;                                                       \
			printf("FAIL %s: %s:%d: %s == %s (%lld != %lld)\n", check_test,      \
			       __FILE__, __LINE__, #a, #b, a_, b_);                          \
		}                                                                            \
	} while (0)

#define CHECK_NEAR(a, b, tol)                                                                \
	do {                                                                                 \
		double a_ = (double)(a), b_ = (double)(b);                                   \
		if (a_ - b_ > (tol) || b_ - a_ > (tol)) {                                    \
			check_fails++;                                                       \
			printf("FAIL %s: %s:%d: %s ~ %s (%g vs %g)\n", check_test, __FILE__, \
			       __LINE__, #a, #b, a_, b_);                                    \
		}                                                                            \
	} while (0)

typedef void (*check_fn)(void);
struct check_case {
	const char *name;
	check_fn fn;
};

#define CASES_END {0, 0}

#endif /* CASE_CHECK_H_ */
