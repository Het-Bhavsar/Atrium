/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include "oc_fast.h"

bool oc_fast_sample(struct oc_fast *f, bool armed, int32_t i_ua, bool sat, int32_t limit_ua)
{
	if (!armed) {
		f->over = 0;
		return false;
	}
	if (sat || i_ua > limit_ua) {
		if (f->over < UINT8_MAX) {
			f->over++;
		}
		if (f->over >= OC_FAST_COUNT) {
			f->over = 0;
			return true;
		}
		return false;
	}
	f->over = 0;
	return false;
}
