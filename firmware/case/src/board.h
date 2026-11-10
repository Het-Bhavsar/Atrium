/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Case board pin map, CH32V003F4U6 QFN-20 (CH32V003DS0 V1.8, table 2-1). QFN pin numbers
 * in brackets. Matches the case netlist (atrium-evt-baseline/hardware/design-data/netlist/case_nets.yaml,
 * 2026-11-08: 18 of 18 MCU pins).
 */
#ifndef CASE_BOARD_H_
#define CASE_BOARD_H_

/* [1] TIM2_CH4, LED blue cathode, sinks. NRST off in the option byte (minichlink -D). */
#define PIN_LED_B  PD7
/* [2] OPN0: Rf 20k from OPO, Rg 1k to GND (x21) */
#define PIN_OPA_N  PA1
/* [3] OPP0: 10k from the shunt top, 1.5M from VDD (pedestal) */
#define PIN_OPA_P  PA2
/* [5] SY6280 EN; also the top of the NTC divider. 1M to GND. */
#define PIN_VIN_EN PD0
/* [7] TIM2_CH3, LED green cathode */
#define PIN_LED_G  PC0
/* [8] 5 V tolerant: VBUS through 10k, 100k to GND. EXTI1. */
#define PIN_VBUS   PC1
/* [9] 5 V tolerant: HAL248 output. EXTI2. */
#define PIN_LID    PC2
/* [10] button to GND, internal pull-up. EXTI3. */
#define PIN_BTN    PC3
/* [11] A2: NTC, TH1 NCU15WF104F6SRC (100k, B 4250 K class) to GND, R20 100k to VIN_EN */
#define PIN_NTC    PC4
#define ADC_CH_NTC 2
/* [12] U5 A (DIR tied to VCCA: A to B) */
#define PIN_SWCLK  PC5
/* [13] U6 A */
#define PIN_SWDIO  PC6
/* [14] U6 DIR: high = A to B (the case drives SWDIO) */
#define PIN_SWDIR  PC7
/* [15] PD1 SWIO: the case's own programming pad */
/* [16] TPS61023 EN, 1M to GND */
#define PIN_BOOST  PD2
/* [17] TIM2_CH2, LED red cathode */
#define PIN_LED_R  PD3
/* [18] OPO, ADC channel 7 */
#define PIN_OPA_O  PD4
#define ADC_CH_OPA 7
/* [19] XC6206P182 VIN (the GPIO powers the LDO: it has no EN pin), 1M to GND */
#define PIN_LDO    PD5
/* [20] BQ24040 CHG, open drain, internal pull-up */
#define PIN_CHRG   PD6

#define ADC_CH_VREFINT 8

/* HAL248 output level with the lid open (no magnet): open drain, off without a magnet, so R19 pulls it high
 * (Hallwee HAL248 data sheet V3.3, p4) */
#define LID_OPEN_LEVEL 1

#endif /* CASE_BOARD_H_ */
