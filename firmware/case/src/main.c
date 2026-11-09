/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 *
 * Atrium charging case: the hardware around case_sm.c. No RTOS: a 100 ms loop while
 * anything needs the MCU (LED frames every 20 ms), standby otherwise.
 *  - Each 20 ms LED frame while the feed is armed: one pod-current reading for the fast
 *    over-current cut-off (oc_fast.h), which turns VIN off itself.
 *  - Each tick: read the lid, button, USB and charger CHG pins, VDD from VREFINT, and,
 *    with the outputs of the last tick, the pod current (OPA, 16 samples), the NTC (VIN on)
 *    and the SWDIO pull-up (1.8 V side on). Step the state machine, then set the outputs.
 *  - Standby: lid, button and VBUS wake it on either edge (EXTI 1-3, port C); the AWU
 *    wakes it for the next timer, in 480 ms steps up to 30.7 s. Standby keeps RAM and pin
 *    states (CH32V003RM 2.3.3). Time asleep is counted in AWU periods; an EXTI wake-up
 *    adds nothing.
 *  - The MCU stays awake for 5 s after reset and whenever USB is in, so the SWIO
 *    programmer can always reach it.
 * Everything electrical here is [VERIFY on the EVT case board].
 */
#include "ch32fun.h"

#include "board.h"
#include "case_sm.h"
#include "led.h"
#include "oc_fast.h"
#include "sense.h"
#include "swd.h"

#define OVS           16
#define TICKS_PER_MS  (FUNCONF_SYSTEM_CORE_CLOCK / 8000)
#define FRAME_MS      20
#define AWU_STEP_MS   480 /* 61440 / 128 kHz [VERIFY LSI tolerance] */
#define AWU_MAX       64  /* window 0-63: up to 30.7 s */
#define BOOT_AWAKE_MS 5000
#define SWD_HOLD      1000  /* idle clocks with CTRL-AP RESET set, about 1 ms */

/* ---- time ---- */

static uint32_t clk_ms, clk_last, clk_rem;

static uint32_t now_ms(void)
{
	uint32_t c = SysTick->CNT;
	uint32_t d = c - clk_last + clk_rem;

	clk_last = c;
	clk_ms += d / TICKS_PER_MS;
	clk_rem = d % TICKS_PER_MS;
	return clk_ms;
}

void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
	SysTick->SR = 0;
}

/* sleep (WFI, peripherals running) until SysTick reaches `target` */
static void nap_until(uint32_t target)
{
	if ((int32_t)(SysTick->CNT - target) >= 0) {
		return;
	}
	SysTick->CMP = target;
	SysTick->SR = 0;
	SysTick->CTLR |= SYSTICK_CTLR_STIE;
	NVIC_EnableIRQ(SysTick_IRQn);
	while ((int32_t)(SysTick->CNT - target) < 0) {
		__WFI();
	}
	SysTick->CTLR &= ~SYSTICK_CTLR_STIE;
}

/* ---- analog ---- */

static void adc_init(void)
{
	RCC->CFGR0 &= ~(0x1Fu << 11); /* ADCCLK = HCLK / 2 = 12 MHz */
	RCC->APB2PCENR |= RCC_APB2Periph_ADC1;
	RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;
	ADC1->RSQR1 = 0;
	ADC1->RSQR2 = 0;
	/* 241 cycles on channels 0-9: the NTC divider is a 50k source */
	ADC1->SAMPTR2 = 0x3FFFFFFF;
	ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL;
	ADC1->CTLR2 |= ADC_RSTCAL;
	while (ADC1->CTLR2 & ADC_RSTCAL) {
	}
	ADC1->CTLR2 |= ADC_CAL;
	while (ADC1->CTLR2 & ADC_CAL) {
	}
}

static uint32_t adc_sum(uint8_t ch)
{
	uint32_t s = 0;

	ADC1->RSQR3 = ch;
	for (int i = 0; i < OVS; i++) {
		ADC1->CTLR2 |= ADC_SWSTART;
		while (!(ADC1->STATR & ADC_EOC)) {
		}
		s += ADC1->RDATAR;
	}
	return s;
}

/* OPA: + on PA2 (OPP0), - on PA1 (OPN0), out on PD4 = ADC channel 7 */
static void opa(bool on)
{
	if (on) {
		EXTEN->EXTEN_CTR = (EXTEN->EXTEN_CTR & ~(EXTEN_OPA_PSEL | EXTEN_OPA_NSEL)) | EXTEN_OPA_EN;
	} else {
		EXTEN->EXTEN_CTR &= ~EXTEN_OPA_EN;
	}
}

/* ---- LED: TIM2 CH2 red PD3, CH3 green PC0, CH4 blue PD7; cathodes, so active low ---- */

static void led_pins(bool pwm)
{
	if (pwm) {
		funPinMode(PIN_LED_R, GPIO_CFGLR_OUT_10Mhz_AF_PP);
		funPinMode(PIN_LED_G, GPIO_CFGLR_OUT_10Mhz_AF_PP);
		funPinMode(PIN_LED_B, GPIO_CFGLR_OUT_10Mhz_AF_PP);
		return;
	}
	funDigitalWrite(PIN_LED_R, 1);
	funDigitalWrite(PIN_LED_G, 1);
	funDigitalWrite(PIN_LED_B, 1);
	funPinMode(PIN_LED_R, GPIO_CFGLR_OUT_10Mhz_PP);
	funPinMode(PIN_LED_G, GPIO_CFGLR_OUT_10Mhz_PP);
	funPinMode(PIN_LED_B, GPIO_CFGLR_OUT_10Mhz_PP);
}

static void pwm_init(void)
{
	RCC->APB1PCENR |= RCC_APB1Periph_TIM2;
	RCC->APB1PRSTR |= RCC_APB1Periph_TIM2;
	RCC->APB1PRSTR &= ~RCC_APB1Periph_TIM2;
	TIM2->PSC = 92; /* 24 MHz / 93 / 256: about 1 kHz */
	TIM2->ATRLR = 255;
	TIM2->CHCTLR1 = TIM_OC2M_2 | TIM_OC2M_1 | TIM_OC2PE;
	TIM2->CHCTLR2 = TIM_OC3M_2 | TIM_OC3M_1 | TIM_OC3PE | TIM_OC4M_2 | TIM_OC4M_1 | TIM_OC4PE;
	TIM2->CCER = TIM_CC2E | TIM_CC2P | TIM_CC3E | TIM_CC3P | TIM_CC4E | TIM_CC4P;
	TIM2->CTLR1 = TIM_ARPE;
	TIM2->SWEVGR = TIM_UG;
	TIM2->CTLR1 |= TIM_CEN;
}

static void led_set(struct rgb c)
{
	TIM2->CH2CVR = c.r;
	TIM2->CH3CVR = c.g;
	TIM2->CH4CVR = c.b;
}

/* ---- SWD through the translators ---- */

static void sw_clk(void *ctx, int level)
{
	(void)ctx;
	funDigitalWrite(PIN_SWCLK, level);
}

static void sw_dio(void *ctx, int level)
{
	(void)ctx;
	funDigitalWrite(PIN_SWDIO, level);
}

static int sw_din(void *ctx)
{
	(void)ctx;
	return funDigitalRead(PIN_SWDIO);
}

/* Never both driving the A side: to drive, turn the translator first, then the pin; to
 * listen, release the pin first, then turn the translator. */
static void sw_dir(void *ctx, int host)
{
	(void)ctx;
	if (host) {
		funDigitalWrite(PIN_SWDIR, 1);
		funPinMode(PIN_SWDIO, GPIO_CFGLR_OUT_10Mhz_PP);
	} else {
		funPinMode(PIN_SWDIO, GPIO_CFGLR_IN_FLOAT);
		funDigitalWrite(PIN_SWDIR, 0);
	}
}

static void sw_delay(void *ctx)
{
	(void)ctx; /* the call overhead at 24 MHz sets the clock [VERIFY rate on a scope] */
}

static const struct swd_io swd_hw = {sw_clk, sw_dio, sw_din, sw_dir, sw_delay, 0};

/* ---- pins ---- */

static void out_low(int pin)
{
	funDigitalWrite(pin, 0);
	funPinMode(pin, GPIO_CFGLR_OUT_10Mhz_PP);
}

static void in_pullup(int pin)
{
	funPinMode(pin, GPIO_CFGLR_IN_PUPD);
	funDigitalWrite(pin, 1);
}

static void board_init(void)
{
	funGpioInitAll();
	out_low(PIN_VIN_EN);
	out_low(PIN_BOOST);
	out_low(PIN_LDO);
	out_low(PIN_SWCLK);
	out_low(PIN_SWDIR);
	funPinMode(PIN_SWDIO, GPIO_CFGLR_IN_FLOAT);
	funPinMode(PIN_OPA_P, GPIO_CFGLR_IN_ANALOG);
	funPinMode(PIN_OPA_N, GPIO_CFGLR_IN_ANALOG);
	funPinMode(PIN_OPA_O, GPIO_CFGLR_IN_ANALOG);
	funPinMode(PIN_NTC, GPIO_CFGLR_IN_ANALOG);
	funPinMode(PIN_VBUS, GPIO_CFGLR_IN_FLOAT); /* 100k to GND on the board */
	funPinMode(PIN_LID, GPIO_CFGLR_IN_FLOAT);  /* push-pull hall output [VERIFY] */
	in_pullup(PIN_BTN);
	in_pullup(PIN_CHRG);
	led_pins(false);

	/* EXTI 1, 2, 3 from port C (VBUS, lid, button), both edges, as wake-up events */
	RCC->APB2PCENR |= RCC_AFIOEN;
	AFIO->EXTICR = (AFIO->EXTICR & ~(0x3Fu << 2)) | (0x2u << 2) | (0x2u << 4) | (0x2u << 6);
	EXTI->EVENR |= EXTI_Line1 | EXTI_Line2 | EXTI_Line3;
	EXTI->RTENR |= EXTI_Line1 | EXTI_Line2 | EXTI_Line3;
	EXTI->FTENR |= EXTI_Line1 | EXTI_Line2 | EXTI_Line3;
	RCC->APB1PCENR |= RCC_APB1Periph_PWR;

	adc_init();
	pwm_init();
	led_pins(true);
}

static uint8_t wake_pins(void)
{
	return (uint8_t)(funDigitalRead(PIN_VBUS) | funDigitalRead(PIN_LID) << 1 |
			 funDigitalRead(PIN_BTN) << 2);
}

/* AWU counts for a timer due in `ms`: rounded up, at most 30.7 s */
static uint32_t awu_counts(uint32_t ms)
{
	uint32_t n = (ms + AWU_STEP_MS - 1) / AWU_STEP_MS;

	return n < 1 ? 1 : n > AWU_MAX ? AWU_MAX : n;
}

/* Standby until a pin changes or the AWU fires. Returns the time asleep, in ms. */
static uint32_t standby(uint32_t wake_in_ms)
{
	uint32_t counts = awu_counts(wake_in_ms);

	led_set((struct rgb){0, 0, 0});
	led_pins(false);
	TIM2->CTLR1 &= ~TIM_CEN;
	opa(false);
	ADC1->CTLR2 &= ~ADC_ADON;

	if (wake_in_ms) {
		RCC->RSTSCKR |= RCC_LSION;
		while (!(RCC->RSTSCKR & RCC_LSIRDY)) {
		}
		EXTI->EVENR |= EXTI_Line9;
		EXTI->FTENR |= EXTI_Line9;
		PWR->AWUPSC = PWR_AWU_Prescaler_61440;
		PWR->AWUWR = counts - 1; /* the window is the value + 1 (RM 2.4.4) */
		PWR->AWUCSR |= 1u << 1;
	} else {
		PWR->AWUCSR &= ~(1u << 1);
		RCC->RSTSCKR &= ~RCC_LSION;
	}
	uint8_t before = wake_pins();

	PWR->CTLR |= PWR_CTLR_PDDS;
	PFIC->SCTLR |= 1u << 2; /* SLEEPDEEP */
	__WFE();
	SystemInit();
	PFIC->SCTLR &= ~(1u << 2); /* WFI naps are plain sleep again */

	uint8_t after = wake_pins();

	adc_init();
	pwm_init();
	led_pins(true);
	clk_last = SysTick->CNT;
	clk_rem = 0;
	return wake_in_ms && after == before ? counts * AWU_STEP_MS : 0;
}

int main(void)
{
	static const struct case_cfg cfg = CASE_CFG_DEFAULT;
	static struct case_sm sm;
	struct case_in in;
	struct case_out out = {0};
	uint32_t zero = 0, idr = 0;
	bool swd_ran = false, swd_ok = false;
	struct oc_fast fast = {0};
	bool fast_trip = false;
	uint16_t vdd_last = 0;

	SystemInit();
	board_init();
	clk_last = SysTick->CNT;

	const uint32_t boot = now_ms();
	uint32_t next = SysTick->CNT;

	case_sm_init(&sm, &cfg, boot);
	for (;;) {
		uint32_t t = now_ms();
		uint16_t vdd = sense_vdd_mv(adc_sum(ADC_CH_VREFINT), OVS);

		in = (struct case_in){
			.now_ms = t,
			.lid_open = funDigitalRead(PIN_LID) == LID_OPEN_LEVEL,
			.btn = !funDigitalRead(PIN_BTN),
			.usb = funDigitalRead(PIN_VBUS),
			.chrg = !funDigitalRead(PIN_CHRG),
			.vbat_mv = vdd,
			.temp_c10 = CASE_TEMP_NONE,
			.swd_done = swd_ran,
			.swd_ok = swd_ok,
			.fast_trip = fast_trip,
		};
		swd_ran = false;
		fast_trip = false;
		vdd_last = vdd;
		if (out.vin_en && out.opa_on) {
			uint32_t s = adc_sum(ADC_CH_OPA);

			in.i_valid = true;
			in.i_ua = sense_pod_ua(s, zero, OVS, vdd);
			in.i_sat = sense_saturated(s, OVS);
		}
		if (out.vin_en) {
			in.temp_c10 = sense_ntc_c10(adc_sum(ADC_CH_NTC), OVS); /* NTC fed by VIN_EN */
		}
		if (out.ldo_pwr) {
			in.pod_pullup = funDigitalRead(PIN_SWDIO);
		}

		case_sm_step(&sm, &in, &out);

		if (out.zero_req) {
			opa(true);
			Delay_Ms(10); /* OPA and the pedestal settle */
			zero = adc_sum(ADC_CH_OPA);
		}
		opa(out.opa_on);
		funDigitalWrite(PIN_VIN_EN, out.vin_en);
		funDigitalWrite(PIN_BOOST, out.boost_en);
		funDigitalWrite(PIN_LDO, out.ldo_pwr);
		if (out.swd_reset) {
			swd_ok = swd_nrf_hard_reset(&swd_hw, SWD_HOLD, &idr) == SWD_OK;
			swd_ran = true;
			funDigitalWrite(PIN_SWCLK, 0); /* idle low, like the nRF's own pull-down */
		}

		if (out.awake || out.led != LED_OFF || t - boot < BOOT_AWAKE_MS) {
			if ((int32_t)(SysTick->CNT - next) > 0) {
				next = SysTick->CNT; /* fell behind (SWD): no catch-up burst */
			}
			for (int f = 0; f < CASE_TICK_MS / FRAME_MS; f++) {
				struct rgb c;

				led_render(out.led, now_ms() - out.led_t0, &c);
				led_set(c);
				/* fast over-current cut-off (oc_fast.h): VIN off within about 40 ms */
				if (out.oc_armed && !fast_trip) {
					uint32_t s = adc_sum(ADC_CH_OPA); /* 16 conversions, about 0.4 ms */
					int32_t ua = sense_pod_ua(s, zero, OVS, vdd_last);

					if (oc_fast_sample(&fast, true, ua, sense_saturated(s, OVS), cfg.oc_ua)) {
						funDigitalWrite(PIN_VIN_EN, 0);
						funDigitalWrite(PIN_LDO, 0);
						funDigitalWrite(PIN_BOOST, 0);
						fast_trip = true;
					}
				} else if (!out.oc_armed) {
					oc_fast_sample(&fast, false, 0, false, cfg.oc_ua);
				}
				next += FRAME_MS * TICKS_PER_MS;
				nap_until(next);
			}
		} else {
			clk_ms += standby(out.wake_in_ms);
			next = SysTick->CNT;
		}
	}
}
