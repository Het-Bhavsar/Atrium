/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Synthetic epoch k, the same in app/python/atrium_ml/logrec.py terms (the vectors in
 * test_log_record.c and test_logpack.c come from logrec.encode and logrec.pack_pages on it).
 */
#ifndef ATRIUM_TEST_LOG_VEC_H_
#define ATRIUM_TEST_LOG_VEC_H_

#include "log_record.h"

static inline struct log_record vec_epoch(int k)
{
	struct log_record r = {
		.bpm = k % 7 ? (float)(50 + k % 60) : 0.0f,
		.conf = (float)(k % 16) / 15.0f,
		.on_skin = k & 1,
		.pulse_ir = k & 2,
		.docked = k & 4,
		.charging = k & 8,
		.rmssd_ms = (float)(k % 300),
		.n_diffs = (uint8_t)(k % 70),
		.cls = (uint8_t)(k % 4),
		.activity_mg = (float)((k * 7) % 300),
		.mlc_max = (uint8_t)(k % 9),
		.pulse_s = (uint8_t)(k % 33),
		.skin_cc = (int16_t)(2400 + (k % 300) * 10),
		.steps = (uint16_t)(k % 300),
		.pitch_deg = (int16_t)((k % 200) - 100),
		.roll_deg = (int16_t)((k % 400) - 200),
		.ox_r = k % 5 == 0 ? (float)(k % 250) / 100.0f : 0.0f,
		.ox_conf = (float)(k % 16) / 15.0f,
	};
	return r;
}

#endif /* ATRIUM_TEST_LOG_VEC_H_ */
