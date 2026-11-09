/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Fit quality for the fit guide's live bar (FEATURES.md #24): port of
 * app/python/atrium_ml/fit.py (POD_CONTRACT.md 5). An engineering signal, not a health value.
 *
 * On the active pulse LED (green, or IR after the headroom swap), every second over the last
 * 4 s of raw counts and of pulse.c's 0.6-3.5 Hz band-passed samples:
 *  - DC = mean raw counts. Saturated when any sample is above 95 % of full scale; dark when DC
 *    is under 1 % with the LED at its maximum (no skin under the window). Either gives q = 0.
 *  - Regularity rho = the highest normalised autocorrelation of the band-passed window at the
 *    lags for 180-40 bpm (8-38 samples), with the n / (n - k) correction.
 *  - Perfusion pi = band-passed RMS x sqrt(rho) / DC: the pulsatile part only.
 *  - q = round(100 x s_pi x s_rho): s_pi ramps on log10 from pi_lo to pi_hi, s_rho from
 *    rho_lo to rho_hi.
 *  - Moving when the accelerometer is above max_motion_mg: q is still sent, the app shows
 *    "hold still".
 *  - Fit good = q >= good_q with a pulse present, not moving, not saturated, not dark. The
 *    app shows green only on it, and confirms the fit when it holds for 8 of the 10 s press.
 * Constants are per pulse LED (green, IR), the same values until V1.
 * Plain C, no Zephyr.
 */
#ifndef ATRIUM_SIGQ_H_
#define ATRIUM_SIGQ_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIGQ_FS     25
#define SIGQ_WIN    100 /* 4 s */
#define SIGQ_LAG_LO 8   /* floor(60 / 180 bpm x 25) */
#define SIGQ_LAG_HI 38  /* ceil(60 / 40 bpm x 25) */

/* Flags, as on the GATT (docs/ble_gatt.md 4.2) */
#define SIGQ_MOVING    0x01
#define SIGQ_SATURATED 0x02
#define SIGQ_DARK      0x04
#define SIGQ_IR        0x08 /* pulse from IR: green has no headroom */
#define SIGQ_PULSE     0x10 /* pulse present (pulse.h, unweighted tracker steady for 5 s) */
#define SIGQ_GOOD      0x20 /* fit good: q >= good_q, pulse, not moving, saturated or dark */

#define SIGQ_NONE 255 /* q when not measuring (optics off, probe mode, under 4 s) */

struct sigq_cfg {
	float pi_lo;
	float pi_hi;
	float rho_lo;
	float rho_hi;
	float max_motion_mg;
	float adc_full;
	float adc_margin;
	float dark_frac;
	uint8_t good_q;
};

/* fit.py FitConfig, every value [ASSUMED] until V1 sets them per LED from temporal-region data */
#define SIGQ_CFG_GREEN                                                            \
	{                                                                         \
		.pi_lo = 3e-4f, .pi_hi = 3e-3f, .rho_lo = 0.4f, .rho_hi = 0.85f,   \
		.max_motion_mg = 30.0f, .adc_full = 524287.0f, .adc_margin = 0.05f, \
		.dark_frac = 0.01f, .good_q = 60,                                  \
	}
#define SIGQ_CFG_IR SIGQ_CFG_GREEN

struct sigq {
	struct sigq_cfg cfg[2]; /* green, IR */
	float bp[SIGQ_WIN];
	int32_t raw[SIGQ_WIN];
	uint16_t head;
	uint16_t n;
};

struct sigq_in {
	float motion_mg; /* band-passed accelerometer RMS (pulse.h motion_mg) */
	bool led_at_max; /* the pulse LED at its highest current */
	bool ir;         /* the pulse LED is IR: its constants and flag */
	bool pulse_present;
};

struct sigq_out {
	uint8_t q;     /* 0..100, SIGQ_NONE */
	uint8_t flags; /* SIGQ_* */
	float pi;      /* pulsatile AC/DC; NAN when not measuring */
	float rho;     /* about 0..1; NAN when not measuring */
};

void sigq_init(struct sigq *s, const struct sigq_cfg *green, const struct sigq_cfg *ir);
void sigq_reset(struct sigq *s);

/* One pulse-LED sample: raw counts and pulse.c's band-passed value for it. */
void sigq_push(struct sigq *s, int32_t raw, float bp);

void sigq_eval(const struct sigq *s, const struct sigq_in *in, struct sigq_out *out);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_SIGQ_H_ */
