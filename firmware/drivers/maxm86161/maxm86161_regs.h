/*
 * Copyright (c) 2026 Local Forge
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAXM86161 register map and register encoding (MAXM86161 data sheet
 * 19-100523 Rev 1, "Register Map" and "Register Details"). The encoding
 * helpers are plain C so the host tests can check them.
 */
#ifndef ATRIUM_MAXM86161_REGS_H_
#define ATRIUM_MAXM86161_REGS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAXM86161_REG_INT_STATUS1 0x00
#define MAXM86161_REG_INT_STATUS2 0x01
#define MAXM86161_REG_INT_EN1     0x02
#define MAXM86161_REG_INT_EN2     0x03
#define MAXM86161_REG_OVF_COUNTER 0x06
#define MAXM86161_REG_FIFO_COUNT  0x07
#define MAXM86161_REG_FIFO_DATA   0x08
#define MAXM86161_REG_FIFO_CFG1   0x09 /* FIFO_A_FULL: A_FULL at 128 - value items */
#define MAXM86161_REG_FIFO_CFG2   0x0A
#define MAXM86161_REG_SYS_CTRL    0x0D
#define MAXM86161_REG_PPG_CFG1    0x11 /* ALC_DISABLE, ADD_OFFSET, PPG1_ADC_RGE[3:2], PPG_TINT[1:0] */
#define MAXM86161_REG_PPG_CFG2    0x12 /* PPG_SR[7:3], SMP_AVE[2:0] */
#define MAXM86161_REG_PPG_CFG3    0x13 /* LED_SETLNG[7:6], DIG_FILT_SEL, BURST_RATE, BURST_EN */
#define MAXM86161_REG_PD_BIAS     0x15
#define MAXM86161_REG_LED_SEQ1    0x20 /* LEDC2[7:4], LEDC1[3:0] */
#define MAXM86161_REG_LED_SEQ2    0x21
#define MAXM86161_REG_LED_SEQ3    0x22
#define MAXM86161_REG_LED1_PA     0x23 /* green */
#define MAXM86161_REG_LED2_PA     0x24 /* IR */
#define MAXM86161_REG_LED3_PA     0x25 /* red */
#define MAXM86161_REG_LED_RANGE1  0x2A /* LED3_RGE[5:4], LED2_RGE[3:2], LED1_RGE[1:0] */
#define MAXM86161_REG_PART_ID     0xFF

#define MAXM86161_PART_ID 0x36

/* INT_STATUS1 / INT_EN1 */
#define MAXM86161_INT_A_FULL   (1U << 7)
#define MAXM86161_INT_DATA_RDY (1U << 6)
#define MAXM86161_INT_ALC_OVF  (1U << 5)
#define MAXM86161_INT_PWR_RDY  (1U << 0)

/* SYS_CTRL */
#define MAXM86161_SINGLE_PPG (1U << 3)
#define MAXM86161_LP_MODE    (1U << 2)
#define MAXM86161_SHDN       (1U << 1)
#define MAXM86161_RESET      (1U << 0)

/* FIFO_CFG2 */
#define MAXM86161_FLUSH_FIFO    (1U << 4)
#define MAXM86161_FIFO_STAT_CLR (1U << 3)
#define MAXM86161_A_FULL_TYPE   (1U << 2)
#define MAXM86161_FIFO_RO       (1U << 1)

#define MAXM86161_FIFO_DEPTH 128
#define MAXM86161_N_SLOTS    6

/* LEDCn codes (Table 2): LED1 green, LED2 IR, LED3 red */
#define MAXM86161_LEDC_NONE  0x0
#define MAXM86161_LEDC_GREEN 0x1
#define MAXM86161_LEDC_IR    0x2
#define MAXM86161_LEDC_RED   0x3
#define MAXM86161_LEDC_DA    0x9 /* direct ambient: photodiode with no LED, no ambient cancel */

/* FIFO tags (Table 3) */
#define MAXM86161_TAG_LEDC1    0x01 /* 0x01..0x06: LEDC1..LEDC6 on PPG1 */
#define MAXM86161_TAG_PF_LEDC1 0x0D /* 0x0D..0x0F: LEDC1..3 after picket-fence replacement */
#define MAXM86161_TAG_INVALID  0x1E

struct maxm86161_slot {
	uint8_t ledc;    /* MAXM86161_LEDC_* */
	uint32_t led_ua; /* LED current, uA; ignored for MAXM86161_LEDC_DA */
};

/* Registers for a slot sequence at one sample rate. */
struct maxm86161_cfg_regs {
	uint8_t seq[3];   /* 0x20..0x22 */
	uint8_t pa[3];    /* 0x23..0x25: green, IR, red */
	uint8_t range1;   /* 0x2A */
	uint8_t ppg_cfg2; /* 0x12 */
};

/* LED range 0..3 (31/62/93/124 mA full scale, LSB = range / 255) and code for a current. */
int maxm86161_led_code(uint32_t led_ua, uint8_t *range, uint8_t *code);
uint32_t maxm86161_led_ua(uint8_t range, uint8_t code);

/* PPG_SR code for an exact supported rate (8, 16, 25, 32, 50, 64, 84, 100, 128, 200, 256 Hz,
 * single pulse). -1 when the rate is not one of them. */
int maxm86161_sr_code(uint16_t hz);

/*
 * Encode up to 6 slots at one rate. A LED may appear in only one slot, since each LED has one
 * PA register; a direct-ambient slot (MAXM86161_LEDC_DA) at most once, with no PA or range.
 * Its FIFO tag is its slot position, like a LED's (data sheet, FIFO Data example).
 * Returns 0, or -1 on a bad slot, a repeated LED or DA slot, or an unsupported rate.
 */
int maxm86161_encode(const struct maxm86161_slot *s, size_t n, uint16_t hz,
		     struct maxm86161_cfg_regs *r);

/* The same with SMP_AVE: hz is PPG_SR, avg (1, 2, 4 ... 128) samples are averaged per channel
 * before the FIFO, which then runs at hz / avg. -1 also for an avg that is not a power of 2. */
int maxm86161_encode_avg(const struct maxm86161_slot *s, size_t n, uint16_t hz, uint8_t avg,
			 struct maxm86161_cfg_regs *r);

/* One 3-byte FIFO item (MSB first): tag in bits 23..19, unsigned 19-bit value. */
static inline void maxm86161_fifo_decode(const uint8_t b[3], uint8_t *tag, uint32_t *value)
{
	uint32_t raw = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];

	*tag = (uint8_t)(raw >> 19);
	*value = raw & 0x7FFFF;
}

/* Slot index (0..5) for a data tag, picket-fence tags included; -1 for status tags. */
int maxm86161_tag_slot(uint8_t tag);

#ifdef __cplusplus
}
#endif

#endif /* ATRIUM_MAXM86161_REGS_H_ */
