/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * 12-byte log record, one per 30 s epoch (docs/ble_gatt.md section 7; the reference codec
 * is app/python/atrium_ml/logrec.py, whose encode() fixes the byte layout). Plain C.
 *
 *  0  u8  pulse, bpm, mean of the epoch's confident seconds; 0 = none
 *  1  u8  bits 0-3 pulse confidence x 15 | bit 4 on skin | bit 5 pulse from IR |
 *         bit 6 docked | bit 7 charging
 *  2  u8  RMSSD, ms (1..254, 254 = 254 or more); 0 = fewer than 5 clean differences
 *  3  u8  bits 0-5 clean successive differences (0-63) | bits 6-7 activity class
 *  4  u8  activity index, mg, saturating
 *  5  u8  bits 0-2 highest MLC motion level | bits 3-7 pulse-present seconds (0-30)
 *  6  u8  skin TH1: (T - 25.0 C) / 0.1, 1..254; 0 = invalid or below 25.05 C
 *  7  u8  steps in the epoch, saturating
 *  8  i8  head pitch, 1 degree; -128 = unknown
 *  9  i8  head roll, 2 degrees; -128 = unknown
 *  10 u8  oxygen-trend burst R x 100 for a burst that ended in the epoch; 0 = none
 *  11 u8  bits 0-3 burst confidence x 15 | bits 4-7 always 0 (marks a written record)
 */
#ifndef ATRIUM_LOG_RECORD_H_
#define ATRIUM_LOG_RECORD_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_RECORD_SIZE 12
#define LOG_EPOCH_S     30
#define LOG_HEADER_SIZE 8
#define LOG_BLOCK_MAX   255
#define LOG_PAGE_SIZE   4096
#define LOG_FORMAT      1
/* time before the phone has set the clock: seconds since boot with this bit set */
#define LOG_TIME_UPTIME (1UL << 31)

#define LOG_ANGLE_UNKNOWN INT16_MIN
#define LOG_SKIN_UNKNOWN  INT16_MIN

struct log_record {
	float bpm;          /* 0 = none */
	float conf;         /* 0..1 */
	bool on_skin;
	bool pulse_ir;
	bool docked;
	bool charging;
	float rmssd_ms;     /* 0 = none */
	uint8_t n_diffs;
	uint8_t cls;        /* enum mlc_class, 0..3 */
	float activity_mg;
	uint8_t mlc_max;    /* 0..7 */
	uint8_t pulse_s;    /* 0..30 */
	int16_t skin_cc;    /* TH1, 0.01 C; LOG_SKIN_UNKNOWN */
	uint16_t steps;
	int16_t pitch_deg;  /* LOG_ANGLE_UNKNOWN */
	int16_t roll_deg;   /* LOG_ANGLE_UNKNOWN */
	float ox_r;         /* 0 = no burst ended in this epoch */
	float ox_conf;
};

void log_record_pack(const struct log_record *r, uint8_t out[LOG_RECORD_SIZE]);
void log_record_unpack(const uint8_t in[LOG_RECORD_SIZE], struct log_record *r);

/* A written record: byte 11 bits 4-7 are 0 (0xF in erased flash). */
static inline bool log_record_written(const uint8_t rec[LOG_RECORD_SIZE])
{
	return (rec[11] >> 4) == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_LOG_RECORD_H_ */
