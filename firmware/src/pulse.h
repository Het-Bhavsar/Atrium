/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pulse rate, "spectral track" (port of app/python/atrium_ml/hr.py, FEATURES.md #1).
 * Wellness estimate only.
 *
 * Green (or IR) PPG and the three accelerometer axes at 25 Hz, band-passed
 * 0.6-3.5 Hz. Every second, the last 8 s go through a Hann window and a
 * 256-point FFT. While the head moves, the accelerometer spectrum is
 * subtracted from the PPG spectrum. Peaks are tracked against the last
 * estimate, and the output moves at most 6 bpm a second.
 *
 * A second, unweighted tracker on the same spectra gives the pulse-presence
 * test for wear detection (atrium_ml/wear.py pulse_present): motion
 * subtraction can make noise look confident, so presence never uses it.
 * RAM about 6.5 KB. Plain C, no Zephyr.
 */
#ifndef ATRIUM_PULSE_H_
#define ATRIUM_PULSE_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PULSE_FS     25 /* the filters are designed for this rate */
#define PULSE_WIN    200
#define PULSE_NFFT   256
#define PULSE_NBIN   (PULSE_NFFT / 2 + 1)

struct pulse_biquad2 {
	float z[2][2];
};

struct pulse_track {
	bool have_est;
	float est;
	int lost;
	bool have_alt;
	float alt;
	int alt_n;
};

/* One second's results. NAN marks "none". */
struct pulse_out {
	float bpm;          /* tracked output, held through low confidence; NAN before the first */
	float conf;         /* 0..1, of the motion-weighted spectrum */
	float peak_bpm;     /* this window's chosen peak */
	float acc_peak_bpm; /* strongest accelerometer peak while moving */
	float motion_mg;
	float raw_conf;     /* unweighted spectrum */
	float raw_peak_bpm;
	bool pulse_present; /* wear's pulse test */
	bool valid;         /* conf >= conf_min */
};

struct pulse_est {
	bool enabled;
	bool primed;
	uint32_t n; /* samples seen */
	struct pulse_biquad2 fx;
	struct pulse_biquad2 fa[3];
	float x[PULSE_WIN]; /* band-passed PPG ring */
	float a[3][PULSE_WIN];
	uint16_t head;
	struct pulse_track trk;     /* with motion subtraction */
	struct pulse_track raw_trk; /* unweighted, for pulse presence */
	float prev_raw_peak;
	int present_run;
	struct pulse_out out;
	bool have_out;
};

/* fs other than 25 Hz disables the estimate (the off-body probe runs at 8 Hz). */
void pulse_init(struct pulse_est *p, uint8_t fs);

/*
 * One sample: PPG counts as the AFE gives them (intensity, so it falls as blood volume
 * rises), and the accelerometer in mg on the same time base (NULL if none). Returns true
 * when a new one-second result is ready.
 */
bool pulse_push(struct pulse_est *p, int32_t ppg_counts, const int16_t acc_mg[3]);

/* Latest result; false before the first 8 s. */
bool pulse_get(const struct pulse_est *p, struct pulse_out *out);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_PULSE_H_ */
