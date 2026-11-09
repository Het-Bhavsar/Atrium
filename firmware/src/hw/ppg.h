/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Optical front end behind one interface. A mode's LED slots go in; samples
 * come out by slot and channel. The rest of the firmware never sees the AFE's
 * registers. The MAXM86161 backend is ppg_maxm86161.c; another AFE needs only
 * another backend file.
 */
#ifndef ATRIUM_PPG_H_
#define ATRIUM_PPG_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "modes.h"

/* One sample: slot index in the mode, channel 0 or 1 (dual photodiode), signed ADC value. */
typedef void (*ppg_sample_cb_t)(uint8_t slot, uint8_t channel, int32_t value);

/* Raw AFE FIFO bytes, for the BLE stream. Always whole words of ppg_raw_word_bytes(). */
typedef void (*ppg_raw_cb_t)(const uint8_t *raw, size_t len);

bool ppg_ready(void);

/* Run a mode's slots. A mode with no slots shuts the AFE down. */
int ppg_apply(const struct mode_cfg *m);

void ppg_set_callbacks(ppg_sample_cb_t sample, ppg_raw_cb_t raw);

/* Size of one raw FIFO word, so BLE packets never split a word. */
size_t ppg_raw_word_bytes(void);

#endif /* ATRIUM_PPG_H_ */
