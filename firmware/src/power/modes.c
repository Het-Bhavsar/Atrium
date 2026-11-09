/*
 * Copyright (c) 2026 Het Bhavsar
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
 *
 * LED currents with the MAXM86161A (data sheet 19-102034 Rev 1). Its green LED returns about
 * 1.7 x the MAXM86161's light at the same current: production-test count 115000 at the 32 uA
 * range against 135000 at 16 uA, both at 124 mA (2.4 x by radiant power, 70 mW at 175 mA against
 * 25 mW at 150 mA). Red and IR give the same counts on both parts. The table keeps green at
 * 15 / 10 / 5 mA, so the extra light becomes signal margin on darker skin (in the
 * hardware/simulation/ppg_optics.md model, Fitzpatrick V-VI meets the per-sample SNR target in
 * 49 % of cases at 15 mA, against 9 %). It does not become battery. At the MAXM86161's light
 * (9 / 6 / 3 mA) green would draw 22.5 uA less in full and smart, 15.0 uA less in sleep, 3.7 uA
 * less in lite and 2.4 uA less in the probe, and that margin would be lost. The budgets below
 * stay at the table currents. The stream contract also fixes the currents (POD_CONTRACT: no LED
 * AGC), so any green trim waits for the bench (docs/open-items.md, O-43).
 * Docked idle and off-body use the s4 model with no optics, CPU 0.5 ms/s, NTCs at
 * 0.1 Hz and advertising every 2 s (3 x 0.4 ms TX).
 *
 * Research modes (depth study, app/python/depth_study): green 15, IR 10 and red 10 mA as in
 * full, plus a direct-ambient (DA) exposure, at 100 or 200 Hz; accelerometer at 104 Hz. All
 * budgets [ASSUMED] until measured on the bench:
 *  - module: 4 exposures per sample (DA counted as one [ASSUMED]) x 1.33 / 2.0 uA:
 *    533 / 800 uA at 100 Hz, 1067 / 1600 uA at 200 Hz;
 *  - LED: 35 mA x 123.8 us x 1.21 per sample (DA has no LED): 524 uA at 100 Hz, 1049 at 200;
 *  - non-optical: full's 206 typ / 236 wc, plus the s4 model's difference for a 30 ms
 *    connection interval, 2.0 / 3.3 kB/s of notifications (PPG 1200 / 2400 B/s, PPG headers
 *    100 / 200 B/s, motion 693 B/s), CPU 10 / 14 ms/s [ASSUMED], 1.8 / 3.0 kB/s of I2C and the
 *    IMU at 104 Hz: +248 / +256 uA at 100 Hz, +385 / +401 uA at 200 Hz.
 *    Totals: 1512 / 1816 uA at 100 Hz, 2706 / 3286 uA at 200 Hz.
 *  The LSM6DSOX accelerometer runs in low-power mode at 12.5 Hz and normal mode at 104 Hz
 *  (accel-pm in atrium_pod.dts; high-performance would be 170 uA at any rate, DS12814 Table 3).
 *  The data sheet gives 26 uA at 50 Hz and 4.5 uA at 1.6 Hz in low-power, and no figure at
 *  12.5 or 104 Hz [VERIFY on the bench]; the IMU terms here are s4's 7 and 17 uA.
 * Stream: 3 bytes x 4 slots x rate = 1200 / 2400 B/s of FIFO words, 8-byte header per
 * notification (32 FIFO items each, so 12.5 / 25 per second). A 30 mAh cell lasts about
 * 20 h / 11 h [ASSUMED]: sessions, not daily wear.
 *
 * Averaged research modes 8 and 9: the same four slots at PPG_SR 200 Hz with SMP_AVE 4 or 8,
 * so the FIFO gets 50 or 25 Hz, each output the mean of adjacent samples per channel (data
 * sheet, PPG Configuration 2). Frame: 4 exposures x tPW 129.8 us (tINT 117.3 + settling 12 +
 * 0.5) at 200 Hz; the data sheet's limit for 4 exposures at tINT 117.3 us is 400 sps. Burst
 * mode would idle between bursts, but its rates (8, 32, 84, 256 Hz) give no 25 or 50 Hz.
 *  - module and LED as mode 7: 1067 / 1600 uA and 1049 uA (exposures still run at 200 Hz);
 *  - non-optical: full's 206 / 236 plus s4's difference for a 100 ms interval, CPU 8 / 7 ms/s
 *    [ASSUMED], PPG 600 / 300 B/s + 50 / 25 B/s of headers, motion 693 B/s: +121 / +125 uA
 *    (mode 8), +86 / +88 uA (mode 9).
 *    Totals: 2443 / 3010 uA (mode 8), 2408 / 2973 uA (mode 9), all [ASSUMED]. Averaging cuts
 *    the stream to a quarter or an eighth of mode 7's, not the current: about 5.5 x full's 437.
 *
 * Vibration modes 10 and 11 (bone-conducted voice and jaw vibration, bench capture): no LEDs
 * and the MAXM86161 shut down; the LSM6DSOX accelerometer alone in high-performance mode at
 * 3333 or 6667 Hz, +-2 g, LPF2 off (bandwidth ODR/2, DS12814 6.4.1), batched in its FIFO and
 * read in bursts of 36 words over I2C at 400 kHz (src/vibcap.h). Stream: the chip Z axis
 * (pod z, normal to the skin) as i16, 2 B per sample: 6667 / 13333 B/s, plus 14 B of header
 * per 115 samples. Budgets [ASSUMED], s4 terms (hardware/simulation/s4_power_energy.py) at the
 * cell, typ (2M PHY) / worst (1M PHY, more CPU):
 *  - accelerometer high-performance 170 uA on the 1.8 V rail (DS12814 Table 3, LA_IddHP, the
 *    same at any ODR): 89 uA at the cell;
 *  - I2C: 7 B per FIFO word, 3 B per burst and 5 B per status poll, 9 bits per byte at 400 kHz:
 *    0.56 s/s at 3333 Hz; 6667 Hz needs more than the bus has (46.7 kB/s against 44.4), so it
 *    runs flat out, 0.92 / 0.98 s/s: 565 uA (mode 10), 933 / 989 uA (mode 11), with s4's
 *    HFCLK, EasyDMA, TWIM and the 4.7 kOhm pull-ups;
 *  - BLE at a 15 ms interval: 7.1 / 12.0 to 14.1 kB/s, 452 / 678 uA (mode 10), 651 / 1187 uA
 *    (mode 11); CPU 30 / 45 ms/s (mode 10), 55 / 80 ms/s (mode 11);
 *  - NTC, protection and the rest: 12 uA.
 *    Totals: 1235 / 1518 uA (mode 10), 1898 / 2587 uA (mode 11). A 30 mAh cell lasts about
 *    24 h or 16 h [ASSUMED]: bench sessions, not daily wear.
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
	/* DA last, as in the data sheet's ambient example: tags 1 green, 2 IR, 3 red, 4 DA */
	[MODE_RESEARCH_100] = {
		.name = "research-100",
		.slot = {S(LED_GREEN, 15), S(LED_IR, 10), S(LED_RED, 10), S(LED_AMBIENT, 0)},
		.n_slots = 4, .hz = 100, .imu_mhz = 104000, .tmp_period_s = 1, .stream_bps = 1200,
		.ci_ms = 30, .budget_typ_ua = 1512, .budget_wc_ua = 1816,
	},
	[MODE_RESEARCH_200] = {
		.name = "research-200",
		.slot = {S(LED_GREEN, 15), S(LED_IR, 10), S(LED_RED, 10), S(LED_AMBIENT, 0)},
		.n_slots = 4, .hz = 200, .imu_mhz = 104000, .tmp_period_s = 1, .stream_bps = 2400,
		.ci_ms = 30, .budget_typ_ua = 2706, .budget_wc_ua = 3286,
	},
	[MODE_RESEARCH_AVG50] = {
		.name = "research-avg50",
		.slot = {S(LED_GREEN, 15), S(LED_IR, 10), S(LED_RED, 10), S(LED_AMBIENT, 0)},
		.n_slots = 4, .hz = 50, .avg = 4, .imu_mhz = 104000, .tmp_period_s = 1,
		.stream_bps = 600, .ci_ms = 100, .budget_typ_ua = 2443, .budget_wc_ua = 3010,
	},
	[MODE_RESEARCH_AVG25] = {
		.name = "research-avg25",
		.slot = {S(LED_GREEN, 15), S(LED_IR, 10), S(LED_RED, 10), S(LED_AMBIENT, 0)},
		.n_slots = 4, .hz = 25, .avg = 8, .imu_mhz = 104000, .tmp_period_s = 1,
		.stream_bps = 300, .ci_ms = 100, .budget_typ_ua = 2408, .budget_wc_ua = 2973,
	},
	/* accelerometer only; .hz is 0 because no PPG runs, imu_mhz is the nominal ODR */
	[MODE_RESEARCH_VIB3K3] = {
		.name = "research-vib3k3",
		.n_slots = 0, .hz = 0, .imu_mhz = 3333333, .tmp_period_s = 1, .stream_bps = 6667,
		.ci_ms = 15, .budget_typ_ua = 1235, .budget_wc_ua = 1518,
	},
	[MODE_RESEARCH_VIB6K7] = {
		.name = "research-vib6k7",
		.n_slots = 0, .hz = 0, .imu_mhz = 6666667, .tmp_period_s = 1, .stream_bps = 13333,
		.ci_ms = 15, .budget_typ_ua = 1898, .budget_wc_ua = 2587,
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
	return (m == MODE_FULL || m == MODE_SMART || mode_is_research(m)) ? MODE_LITE : m;
}

bool mode_is_research(enum pod_mode m)
{
	return m == MODE_RESEARCH_100 || m == MODE_RESEARCH_200 || m == MODE_RESEARCH_AVG50 ||
	       m == MODE_RESEARCH_AVG25 || mode_is_vibration(m);
}

bool mode_is_vibration(enum pod_mode m)
{
	return m == MODE_RESEARCH_VIB3K3 || m == MODE_RESEARCH_VIB6K7;
}

uint8_t mode_avg(const struct mode_cfg *m)
{
	return m->avg > 1 ? m->avg : 1;
}

uint16_t mode_sample_hz(const struct mode_cfg *m)
{
	return (uint16_t)(m->hz * mode_avg(m));
}

bool mode_user_selectable(enum pod_mode m)
{
	return m <= MODE_SLEEP || mode_is_research(m);
}

uint8_t mode_algo_decim(const struct mode_cfg *m)
{
	return (m->hz > MODE_ALGO_HZ && m->hz % MODE_ALGO_HZ == 0) ? m->hz / MODE_ALGO_HZ : 1;
}

uint8_t mode_imu_decim(const struct mode_cfg *m)
{
	uint32_t k = m->imu_mhz / MODE_ALGO_IMU_MHZ;

	return k > 255 ? 255 : k > 1 ? (uint8_t)k : 1;
}

uint8_t mode_stream_tag(const struct mode_cfg *m, uint8_t led)
{
	for (uint8_t i = 0; i < m->n_slots; i++) {
		if (m->slot[i].led == led) {
			return i + 1;
		}
	}
	return 0;
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
