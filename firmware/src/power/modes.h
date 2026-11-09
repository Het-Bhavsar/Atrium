/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Measurement modes and their current budgets, Rev B.2 (MAXM86161). The
 * module runs all of a mode's LEDs at one sample rate, and each LED at most
 * once per sample. Slots, rates and duty follow the ML data contract (green at
 * 25 Hz in every worn mode, accelerometer at 12.5 Hz), except the research modes. Budgets are uA at the
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

/* MAXM86161 LEDs: LED1 green, LED2 IR, LED3 red. LED_AMBIENT is the direct-ambient exposure:
 * the photodiode sampled with no LED and no ambient cancellation (research modes only). */
enum led_id {
	LED_GREEN = 0,
	LED_IR = 1,
	LED_RED = 2,
	LED_AMBIENT = 3,
};

#define MODE_MAX_SLOTS 4

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
	/* research only (depth study): green, IR, red and direct ambient, IMU at 104 Hz. Set
	 * from the Mode characteristic for one connection, never stored, never the default */
	MODE_RESEARCH_100,
	MODE_RESEARCH_200,
	/* the same slots sampled at 200 Hz and averaged on the module (SMP_AVE) to 50 or 25 Hz */
	MODE_RESEARCH_AVG50,
	MODE_RESEARCH_AVG25,
	/* research only (bone-conducted voice and jaw vibration, bench capture): accelerometer
	 * alone in high-performance mode at 3.33 or 6.66 kHz, one axis streamed on Vibration Data;
	 * no LEDs, no gyroscope (src/vibcap.h) */
	MODE_RESEARCH_VIB3K3,
	MODE_RESEARCH_VIB6K7,
	MODE_COUNT,
};

struct mode_cfg {
	const char *name;
	struct led_slot slot[MODE_MAX_SLOTS];
	uint8_t n_slots;
	uint8_t hz;            /* FIFO output rate for every slot: the stream's and filters' rate */
	uint8_t avg;           /* samples averaged on the module per output (SMP_AVE); 0 = 1 */
	uint8_t optics_on_s;   /* optics duty: on this long ... */
	uint8_t optics_period_s; /* ... in every period; 0 = always on */
	uint32_t imu_mhz;      /* accelerometer ODR in mHz, 0 = power-down */
	uint16_t tmp_period_s; /* NTC reading period */
	uint16_t stream_bps;   /* raw PPG (vibration in modes 10, 11) stream payload, bytes/s */
	uint16_t ci_ms;        /* BLE connection interval, or advertising interval when idle */
	uint16_t budget_typ_ua;
	uint16_t budget_wc_ua;
};

/* The docked-and-full idle mode must stay at or below 0.07 mA (SCHEMATIC_SIM.md 5.4 item 2). */
#define DOCKED_IDLE_LIMIT_UA 70

/* Green stays on the MAXM86161's 31 mA range, which is the only one green can reach
 * from the cell (REV_B2_COMPACT.md, green headroom). */
#define GREEN_MAX_MA 30

/* The on-pod pulse, HRV and fit filters run at this rate; faster modes are averaged down. */
#define MODE_ALGO_HZ      25
/* The step rhythm and MLC timing run at this accelerometer rate; faster modes decimate. */
#define MODE_ALGO_IMU_MHZ 12500

const struct mode_cfg *mode_get(enum pod_mode m);

/* A mode the user may choose with the Mode characteristic: 0 to 3, or a research mode. */
bool mode_user_selectable(enum pod_mode m);
bool mode_is_research(enum pod_mode m);

/* Research modes 10 and 11: the accelerometer FIFO capture (src/vibcap.h), no optics. */
bool mode_is_vibration(enum pod_mode m);

/* Samples averaged on the module per FIFO output (1 unless SMP_AVE is used). */
uint8_t mode_avg(const struct mode_cfg *m);

/* The module's own sample rate (PPG_SR): hz x avg. */
uint16_t mode_sample_hz(const struct mode_cfg *m);

/* PPG samples averaged into one for the 25 Hz filters: hz / 25 when that is whole, else 1. */
uint8_t mode_algo_decim(const struct mode_cfg *m);

/* Accelerometer samples per step-rhythm sample: about imu_mhz / 12.5 Hz, 1 to 255. */
uint8_t mode_imu_decim(const struct mode_cfg *m);

/* FIFO tag of a LED's slot in the stream (slot index + 1, MAXM86161 Table 3); 0 if absent. */
uint8_t mode_stream_tag(const struct mode_cfg *m, uint8_t led);

/* The lower-power mode used at night or on a low battery (research modes included). */
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
