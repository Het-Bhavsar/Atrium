/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * ADC readings to physical units, for the case. Plain C, no hardware.
 *  - The ADC is 10-bit and its reference is VDD, which is the case cell itself
 *    (CH32V003DS0 3.3.14: input range 0 to VDD). So the cell voltage comes from VREFINT
 *    (channel 8, 1.17-1.23 V, typ 1.2 V, DS table 3-5), not from a divider.
 *  - Pod current: a 1 ohm low-side shunt in the pogo GND return, the internal OPA at x21
 *    (Rf 20k, Rg 1k), and a pedestal (10k from the shunt, 1.5M from VDD into OPA+) that
 *    keeps the output above 0 V whatever the OPA offset (+-13 mV max, DS table 3-26). The
 *    pedestal (about 525 mV at the output) scales with VDD like the reference, so it is
 *    fixed in ADC counts, and a reading with VIN off before each feed is the zero. The
 *    offset is a fixed voltage, so as VDD moves after the zero it drifts by about 0.1 mA
 *    per 100 mV (typ 3 mV), 0.35 mA at the 13 mV maximum: small against 2.5 mA.
 *  - NTC: TH1 Murata NCP15WF104F03RC (100k, B 4250 K) to GND, R20 100k to VIN_EN
 *    (powered only while feeding) [VERIFY B against Murata's R-T table].
 * Readings are sums of `n` conversions (oversampling).
 */
#ifndef CASE_SENSE_H_
#define CASE_SENSE_H_

#include <stdbool.h>
#include <stdint.h>

#define SENSE_FS          1024 /* 10-bit codes */
#define SENSE_VREFINT_MV  1200 /* typ; 1170-1230 [VERIFY per unit, or trim at test] */
#define SENSE_GAIN        21
#define SENSE_SHUNT_MOHM  1000
#define SENSE_SAT_CODE    1015 /* an average code at or above this is clipped */
#define SENSE_TEMP_NONE   INT16_MIN

/* VDD in mV from a VREFINT sum of n codes. 0 if the sum is 0. */
uint16_t sense_vdd_mv(uint32_t vref_sum, uint8_t n);

/* Pod current in uA from a shunt sum, the zero sum taken with VIN off (same n), and VDD. */
int32_t sense_pod_ua(uint32_t sum, uint32_t zero_sum, uint8_t n, uint16_t vdd_mv);

/* The OPA output is at the top of the ADC range: the current is above full scale. */
bool sense_saturated(uint32_t sum, uint8_t n);

/* Temperature in 0.1 C from an NTC sum, SENSE_TEMP_NONE for an open or shorted NTC. */
int16_t sense_ntc_c10(uint32_t sum, uint8_t n);

/* Case cell charge 0-100 % from its resting voltage [ASSUMED generic LiPo curve]. */
uint8_t sense_batt_pct(uint16_t mv);

#endif /* CASE_SENSE_H_ */
