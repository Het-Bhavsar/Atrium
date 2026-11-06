/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 */
#include "modes.h"

#include <stddef.h>

#define S(l, m) {.led = (l), .ma = (m)}

/*
 * Budget = s4's non-optical current for the matching B.1 mode (CPU, I2C, BLE, IMU, gauge,
 * protection; conservative for the B.2 3.3 V DC/DC rail [VERIFY]), plus the MAXM86161
 * figures from its data sheet:
 *  - module: 400 uA typ / 600 uA max at 100 sps x 3 exposures with the LEDs off,
 *    i.e. 1.33 / 2.0 uA per exposure per sps;
 *  - LED: I x 123.8 us x 1.21 per pulse (one LED at full scale and 100 sps is 600 uA).
 * Slots, rates and lite's 30 s in 120 s duty: ML data contract (ml_firmware_handoff).
 * Docked idle and off-body use the s4 model with no optics, CPU 0.5 ms/s, NTCs at
 * 0.1 Hz and advertising every 2 s (3 x 0.4 ms TX).
 */
static const struct mode_cfg modes[MODE_COUNT] = {
	[MODE_FULL] = {
		.name = "full",
		.slot = {S(LED_GREEN, 15), S(LED_IR, 10), S(LED_RED, 10)},
		.n_slots = 3, .hz = 25, .imu_mhz = 12500, .tmp_period_s = 1, .stream_bps = 225,
		.ci_ms = 200, .budget_typ_ua = 437, .budget_wc_ua = 517,
	},
	[MODE_SMART] = {
		.name = "smart",
		.slot = {S(LED_GREEN, 15)},
		.n_slots = 1, .hz = 25, .imu_mhz = 12500, .tmp_period_s = 1, .stream_bps = 75,
		.ci_ms = 400, .budget_typ_ua = 215, .budget_wc_ua = 247,
	},
	[MODE_LITE] = {
		.name = "lite",
		.slot = {S(LED_GREEN, 10)},
		.n_slots = 1, .hz = 25, .optics_on_s = 30, .optics_period_s = 120,
		.imu_mhz = 12500, .tmp_period_s = 1, .stream_bps = 75, .ci_ms = 1000,
		.budget_typ_ua = 70, .budget_wc_ua = 80,
	},
	[MODE_SLEEP] = {
		.name = "sleep",
		.slot = {S(LED_GREEN, 10)},
		.n_slots = 1, .hz = 25, .imu_mhz = 12500, .tmp_period_s = 10, .stream_bps = 0,
		.ci_ms = 4000, .budget_typ_ua = 129, .budget_wc_ua = 154,
	},
	[MODE_DOCKED_IDLE] = {
		.name = "docked-idle",
		.n_slots = 0, .hz = 0, .imu_mhz = 0, .tmp_period_s = 10, .stream_bps = 0,
		.ci_ms = 2000, .budget_typ_ua = 17, .budget_wc_ua = 25,
	},
	[MODE_OFFBODY] = {
		.name = "off-body",
		.slot = {S(LED_GREEN, 5)},
		.n_slots = 1, .hz = 8, .imu_mhz = 0, .tmp_period_s = 10, .stream_bps = 0,
		.ci_ms = 2000, .budget_typ_ua = 34, .budget_wc_ua = 48,
	},
};

const struct mode_cfg *mode_get(enum pod_mode m)
{
	return (unsigned int)m < MODE_COUNT ? &modes[m] : NULL;
}

enum pod_mode mode_sleep_of(enum pod_mode m)
{
	(void)m;
	return MODE_SLEEP;
}

enum pod_mode mode_lite_of(enum pod_mode m)
{
	return (m == MODE_FULL || m == MODE_SMART) ? MODE_LITE : m;
}

uint8_t mode_green_to_ir(const struct mode_cfg *m, struct mode_cfg *out)
{
	bool has_ir = false;

	for (uint8_t i = 0; i < m->n_slots; i++) {
		has_ir |= m->slot[i].led == LED_IR;
	}
	*out = *m;
	out->n_slots = 0;
	for (uint8_t i = 0; i < m->n_slots; i++) {
		struct led_slot sl = m->slot[i];

		if (sl.led == LED_GREEN) {
			if (has_ir) {
				continue;
			}
			sl.led = LED_IR;
		}
		out->slot[out->n_slots++] = sl;
	}
	return out->n_slots;
}

bool mode_optics_on(const struct mode_cfg *m, uint32_t t)
{
	if (m->n_slots == 0) {
		return false;
	}
	return m->optics_period_s == 0 || (t % m->optics_period_s) < m->optics_on_s;
}
