/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * 24-byte log record, little-endian (docs/ble_gatt.md section 7). Plain C.
 */
#ifndef ATRIUM_LOG_RECORD_H_
#define ATRIUM_LOG_RECORD_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_RECORD_SIZE 24

struct log_record {
	uint32_t time;
	uint8_t bpm;
	uint8_t bpm_conf;
	int16_t skin_cc;    /* TH1, skin side */
	int16_t top_cc;     /* TH4, top side */
	uint16_t activity_mg;
	uint16_t vbat_mv;
	uint8_t soc;
	uint8_t mode;
	bool docked;
	bool charging;
	bool pulse_ir;      /* pulse from IR (green had no headroom) */
	uint16_t rmssd_x10; /* 0.1 ms, 0 = not enough clean beats */
	uint8_t rr_diffs;
	int16_t pitch_deg;  /* head pitch, -90..90; INT16_MIN = unknown */
	int16_t roll_deg;   /* head roll, -180..180; INT16_MIN = unknown */
	uint8_t mlc_max;    /* MLC motion level, 0..7 */
	uint8_t mlc_median;
	uint8_t mlc_class;  /* enum mlc_class */
	uint8_t steps_pm;   /* step rhythm, steps/min; 0 = none */
	uint8_t pulse_s;    /* seconds with a pulse present, 0..15 */
	float ox_conf;      /* oxygen-trend burst confidence 0..1; 0 = no burst ended here */
	float ox_r;         /* burst red/IR ratio; 0 = none */
};

/* Byte value for an unknown angle. */
#define LOG_ANGLE_UNKNOWN (-128)

void log_record_pack(const struct log_record *r, uint8_t out[LOG_RECORD_SIZE]);
void log_record_unpack(const uint8_t in[LOG_RECORD_SIZE], struct log_record *r);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_LOG_RECORD_H_ */
