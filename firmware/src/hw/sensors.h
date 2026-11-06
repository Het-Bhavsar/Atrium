/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Measurement mode -> PPG slots (ppg.h) and LSM6DSOX rate.
 */
#ifndef ATRIUM_SENSORS_H_
#define ATRIUM_SENSORS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "activity.h"
#include "hrv.h"
#include "pulse.h"
#include "modes.h"
#include "spo2.h"

typedef void (*sensors_ppg_sink_t)(const uint8_t *raw, size_t len);

/* Accelerometer samples in mg (x, y, z), the first one taken at t_first_ms of uptime. */
typedef void (*sensors_motion_sink_t)(const int16_t (*xyz)[3], size_t n, uint32_t t_first_ms,
				      uint16_t rate_mhz);

#define SENSORS_MOTION_BATCH 12

int sensors_init(void);

/*
 * Apply a mode. MODE_DOCKED_IDLE puts the AFE and IMU in shutdown. With green_ok
 * false (headroom.h), green swaps to IR. With optics_on false (the off phase of a
 * duty-cycled mode), the optics stop and the IMU keeps its rate.
 */
int sensors_set_mode(enum pod_mode mode, bool green_ok, bool optics_on);

/* Raw accelerometer batches to BLE while non-NULL. */
void sensors_set_motion_sink(sensors_motion_sink_t sink);

/* Raw AFE FIFO bytes to BLE while non-NULL. */
void sensors_set_ppg_sink(sensors_ppg_sink_t sink);

/* Mean green level since the last call (ambient-subtracted ADC counts). False when no samples. */
bool sensors_green_dc(int32_t *dc);

bool sensors_imu_ready(void);

/* Latest one-second pulse result from the pulse source slot; false before the first. */
bool sensors_pulse(struct pulse_out *out);

/* Mean head angle since the last call (one log interval); INT16_MIN when the IMU was off. */
void sensors_posture_close(int16_t *pitch_deg, int16_t *roll_deg);

/* Close the HRV window (one log interval). */
struct hrv_window sensors_hrv_close(void);

/* Close the activity minute. */
struct activity_minute sensors_activity_minute(void);

/* A new MLC motion level (0..7) since the last call, about every 3.12 s while the IMU runs. */
bool sensors_mlc_poll(uint8_t *level);

/* Close one log record's motion: MLC levels and the step rhythm of the last 8 s. */
struct sensors_motion {
	uint8_t max;
	uint8_t median;
	uint8_t cls;      /* enum mlc_class */
	uint8_t steps_pm; /* 0 when still or no rhythm */
};
struct sensors_motion sensors_motion_close(void);

/*
 * Start a 30 s oxygen-trend burst: IR and red join the mode's green slot at 25 Hz.
 * -ENOTSUP unless the applied mode is one green slot at 25 Hz. A mode change cancels it.
 */
int sensors_spo2_start(void);

/* True once when a burst has finished: its result, and the mode's own slots restored. */
bool sensors_spo2_poll(struct spo2_out *out);

bool sensors_spo2_active(void);

#endif /* ATRIUM_SENSORS_H_ */
