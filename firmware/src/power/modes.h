/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Measurement modes and their current budgets, Rev B.2 (MAXM86161). The
 * module runs all of a mode's LEDs at one sample rate, and each LED at most
 * once per sample. Slots, rates and duty follow the ML data contract (green at
 * 25 Hz in every worn mode, accelerometer at 12.5 Hz). Budgets are uA at the
 * cell: the s4 model's non-optical currents (hardware/simulation/s4_power_energy.py),
 * plus MAXM86161 figures from its data sheet (see modes.c). Plain C, no Zephyr.
 */
#ifndef ATRIUM_MODES_H_
#define ATRIUM_MODES_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MAXM86161 LEDs: LED1 green, LED2 IR, LED3 red. */
enum led_id {
	LED_GREEN = 0,
	LED_IR = 1,
	LED_RED = 2,
};

#define MODE_MAX_SLOTS 3

struct led_slot {
	uint8_t led; /* enum led_id */
	uint8_t ma;  /* LED current, mA */
};

/* The value of each mode is what the GATT interface reports (docs/ble_gatt.md). */
enum pod_mode {
	MODE_FULL = 0,
	MODE_SMART,
	MODE_LITE,
	MODE_SLEEP,
	MODE_DOCKED_IDLE, /* on the dock: no optics, no IMU */
	MODE_OFFBODY,     /* wear detection: slow green probe only (FEATURES.md #18) */
	MODE_COUNT,
};

struct mode_cfg {
	const char *name;
	struct led_slot slot[MODE_MAX_SLOTS];
	uint8_t n_slots;
	uint8_t hz;            /* sample rate for every slot */
	uint8_t optics_on_s;   /* optics duty: on this long ... */
	uint8_t optics_period_s; /* ... in every period; 0 = always on */
	uint16_t imu_mhz;      /* accelerometer ODR in mHz, 0 = power-down */
	uint16_t tmp_period_s; /* NTC reading period */
	uint16_t stream_bps;   /* raw PPG stream payload when enabled, bytes/s */
	uint16_t ci_ms;        /* BLE connection interval, or advertising interval when idle */
	uint16_t budget_typ_ua;
	uint16_t budget_wc_ua;
};

/* The docked-and-full idle mode must stay at or below 0.07 mA (SCHEMATIC_SIM.md 5.4 item 2). */
#define DOCKED_IDLE_LIMIT_UA 70

/* Green stays on the MAXM86161's 31 mA range, which is the only one green can reach
 * from the cell (REV_B2_COMPACT.md, green headroom). */
#define GREEN_MAX_MA 30

const struct mode_cfg *mode_get(enum pod_mode m);

/* The lower-power mode used at night or on a low battery. */
enum pod_mode mode_sleep_of(enum pod_mode m);
enum pod_mode mode_lite_of(enum pod_mode m);

/*
 * Copy m with green swapped for IR at the same current, for a cell below the green
 * headroom limit (headroom.h). If the mode already has IR, green is dropped. Returns the slots.
 */
uint8_t mode_green_to_ir(const struct mode_cfg *m, struct mode_cfg *out);

/* Whether a duty-cycled mode's optics run at second `t` of uptime. */
bool mode_optics_on(const struct mode_cfg *m, uint32_t t);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_MODES_H_ */
