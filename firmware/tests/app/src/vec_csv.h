/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Reference-vector CSVs for the host tests, compiled in as text.
 */
#ifndef ATRIUM_TESTS_VEC_CSV_H_
#define ATRIUM_TESTS_VEC_CSV_H_

#include <math.h>
#include <stdlib.h>

/* A tiny CSV reader: header line skipped, fields by index, empty = NAN, text = 0 */

#define MAXF 12

static const char *next_line(const char *p)
{
	while (*p && *p != '\n') {
		p++;
	}
	return *p ? p + 1 : NULL;
}

static int fields(const char *line, double *out)
{
	int n = 0;
	const char *p = line;

	while (p && *p && *p != '\n' && n < MAXF) {
		char *end;

		if (*p == ',' || *p == '\r') {
			out[n++] = NAN;
		} else {
			out[n++] = strtod(p, &end);
			p = end;
		}
		while (*p && *p != ',' && *p != '\n') {
			p++;
		}
		if (*p == ',') {
			p++;
			if (*p == '\n' || *p == 0) {
				out[n++] = NAN; /* trailing empty field */
			}
		}
	}
	return n;
}

#endif /* ATRIUM_TESTS_VEC_CSV_H_ */
