# Charging case firmware

Firmware for the Atrium charging case (see [`docs/charging-case.md`](../../docs/charging-case.md)): a CH32V003F4U6 (WCH RISC-V, QFN-20, LCSC C5299908) in C, with no RTOS. It builds, and the host tests pass. It has not run on a case board yet: see [What needs hardware](#what-needs-hardware).

| Folder | Contents |
|---|---|
| `src/case_sm.c` | Behaviour: feeding, pod state, faults, button, LED choice, standby. Plain C |
| `src/swd.c` | SWD host as far as the nRF52832 CTRL-AP: line reset, DP and AP access, hard reset. Plain C over pin callbacks |
| `src/sense.c`, `src/led.c` | ADC readings to µA, mV, °C and %; the LED patterns as colour over time. Plain C |
| `src/main.c`, `src/board.h` | Hardware: pins, ADC and OPA, TIM2 PWM, translators, EXTI and AWU standby |
| `tests/` | Host tests, a bit-level SW-DP target model (`swd_sim.c`) and a small runner |
| `tools/` | The toolchain image and its script |

## Toolchain

[ch32fun](https://github.com/cnlohr/ch32fun) (MIT), pinned at `b28d285`, with Ubuntu 24.04's `gcc-riscv64-unknown-elf` (rv32ec, ilp32e). It is fetched into the Docker image, not stored in the repo. It was chosen over the vendor SDK because it is bare-metal headers with a plain Makefile, small (the image is 7.4 KB of 16 KB flash, 200 B of 2 KB RAM), and flashes over SWIO with `minichlink` or a WCH-LinkE.

| Script | Usage | Does |
|---|---|---|
| `tools/case.sh` | `firmware/case/tools/case.sh image` | Builds the toolchain image once |
| | `firmware/case/tools/case.sh test` | The host tests in the image |
| | `firmware/case/tools/case.sh build` | Writes `build/atrium_case.{bin,hex,elf}` and prints the size |
| | `firmware/case/tools/case.sh shell` | Opens a shell in the image |
| `Makefile` | `make -C firmware/case test` | The host tests with the local C compiler (ASan and UBSan on) |

`build/` is git-ignored. The host tests also compile the pod's `src/power/vinpat.c`, so the case's VIN pulses are checked against the pod's own detector.

**Flashing.** `minichlink -w firmware/case/build/atrium_case.bin flash -b` over the SWIO pad (PD1), then `minichlink -D` once, so PD7 works as a GPIO (the blue LED) and not as NRST. The MCU stays awake for 5 s after a reset and whenever USB is in, because SWIO does not answer in standby.

## Pins (QFN-20)

From CH32V003DS0 V1.8, table 2-1. The OPA pins are fixed by the silicon. The case netlist (`hardware/design-data/netlist/case_nets.yaml`, 2026-11-08) matches all 18 MCU pins.

Board values the firmware depends on, from that netlist: TP4054 PROG 4.7k (about 213 mA [VERIFY]), boost 5.21 V (768k / 100k), SY6280 ISET 15k (0.45 A backstop above the 80 mA firmware trip), LED XL-1615RGBC-RF common anode to VDD with red 1k and green and blue 330 Ω, HAL248 pull-up R19 4.7M fitted only if the output is open drain (DNP).

| Pin | Port | Net | Notes |
|---|---|---|---|
| 1 | PD7 | LED_B | TIM2_CH4, cathode. NRST turned off in the option byte |
| 2 | PA1 | OPA_N | OPN0: Rf 20k from PD4, Rg 1k to GND (×21) |
| 3 | PA2 | OPA_P | OPP0: 10k from the shunt, 1.5M from VDD (pedestal) |
| 5 | PD0 | VIN_EN | SY6280 EN, and the top of the NTC divider; 1M to GND |
| 7 | PC0 | LED_G | TIM2_CH3, cathode |
| 8 | PC1 | VBUS_DET | 5 V tolerant; 10k from VBUS, 100k to GND; EXTI1 |
| 9 | PC2 | LID | HAL248 output, 5 V tolerant; EXTI2 |
| 10 | PC3 | BTN | to GND, internal pull-up; EXTI3 |
| 11 | PC4 | NTC | ADC A2: TH1 NCP15WF104F03RC (100k, B 4250 K [VERIFY against Murata's R-T table]) to GND, R20 100k to VIN_EN |
| 12 | PC5 | SWDCLK | U5 A; U5 DIR tied to VCCA |
| 13 | PC6 | SWDIO | U6 A |
| 14 | PC7 | SWDIO_DIR | U6 DIR: high when the case drives |
| 15 | PD1 | SWIO | the case's own programming pad |
| 16 | PD2 | BOOST_EN | TPS61023 EN; 1M to GND |
| 17 | PD3 | LED_R | TIM2_CH2, cathode |
| 18 | PD4 | OPA_OUT | OPO, read inside the chip as ADC channel 7 |
| 19 | PD5 | LDO_PWR | powers the XC6206P182 (it has no EN pin); 1M to GND |
| 20 | PD6 | CHRG | TP4054 CHRG, open drain, internal pull-up |

Changes from the spec's block diagram that the schematic must carry:
- **Shunt moved to the low side.** The 1 Ω shunt sits in the pogo GND return. The OPA input range is 0 to VDD, and VDD is the cell (≤ 4.2 V), so it cannot sense at 5.2 V.
- **External gain resistors.** The OPA is a plain op-amp with no internal gain, so ×21 comes from Rf 20k and Rg 1k.
- **Pedestal.** OPA offset is up to ±13 mV (DS table 3-26), which is ±13 mA on 1 Ω. A pedestal from VDD keeps the output above 0 V. It moves with VDD, as the ADC reference does, so a reading with VIN off is the zero.
- **No VBAT divider.** The ADC reference is VDD, which is the cell, so a divider of the cell always reads the same. The cell voltage comes from VREFINT instead (ADC channel 8, 1.17–1.23 V, DS table 3-5): ±2.5 %, so the 3.4 V cut sits between 3.32 and 3.49 V [VERIFY per unit, or trim at test].
- **Presence from SWDIO.** At ×21 the 10-bit ADC resolves about 0.2 mA, which cannot see 80 µA. The case therefore reads SWDIO: 1M to GND on the pod side of U6, and the nRF52832's own pull-up [VERIFY] reads high when a powered pod sits on the pins.

## Behaviour

| Rule | Implementation | Test |
|---|---|---|
| Feed the pod | VIN on when the lid opens (2 min with no pod), while the pod charges, on USB, and for a top-up of a full pod every 4 h [ASSUMED]. Boost only without USB. The current is zeroed with VIN off before each feed; readings until 100 ms after VIN on are ignored | `full, VIN off, top-up`, `full on USB stays fed`, `lid open idle stops feeding` |
| 1.8 V side | LDO_PWR 100 ms after VIN on; off with VIN and through every drop-out | checked on every tick of every state-machine test |
| Pod state | Present: SWDIO high or > 0.4 mA; absent 2 s: no pod. Charging > 5 mA; full < 2.5 mA for 30 s, after which VIN goes off without USB and with the lid shut | `pod removal is seen`, `present by the SWDIO pull-up` |
| Over-current | > 80 mA (or a clipped reading) 3 times in a row: VIN off, red fast blink, retry in 30 s. SY6280 ISET (0.4 A) is the hardware limit | `over-current trips and retries`, `inrush is blanked` |
| Case cell | Below 3.4 V 3 times without USB: no feeding until 3.6 V or USB; red blinks on lid open | `low case battery` |
| Temperature | NTC ≥ 45 °C while feeding: VIN off, retry in 10 min, and it stays off until a reading below 42 °C | `hot case cell` |
| Case signature | Owner decision 2026-11-08. 3 s after VIN on with a pod there, and after each reset: two 200 ms VIN drop-outs. The pod reports "fed by the case" (Charge State flag bit 4) | `soft reset`, `a day of top-ups and lid opens` (no reset in 24 h) |
| Soft reset | Hold 5–12 s, release: three 200 ms drop-outs 300 ms apart (1.2 s), at least 2.5 s clear of the signature. The pod reboots on the third | `soft reset reboots the pod` (through the pod's detector) |
| Hard reset | Hold ≥ 12 s, release. Once the 1.8 V side has been on 100 ms: line reset, JTAG-to-SWD, DPIDR, ABORT, debug power-up, CTRL-AP IDR = 0x02880000 [VERIFY], RESET 1 then 0, debug power-up released. 2 slow red blinks if it fails [ASSUMED] | `hard reset`, `hard reset with VIN off`, swd suite |
| LED | Spec patterns: amber breathe 5 s, green 5 s, battery 3/2/1 blinks at > 60 / 20–60 / < 20 %, red fast blink, white flash and double flash at 5 s and 12 s of a hold. USB in: amber or green steady, below any pod pattern. Colours [ASSUMED] | led suite, `short press`, `full pod shows green at once` |
| Standby | When nothing needs the MCU. Lid, button and VBUS wake it (EXTI on both edges). The AWU wakes it for the next timer, in 480 ms steps up to 30.7 s | `boot with no pod sleeps` |

SWD runs at the speed of the call overhead at 24 MHz [VERIFY on a scope]. ERASEALL is not wired to the button; `swd_ap_write(io, NRF_CTRLAP, NRF_CTRLAP_ERASEALL, 1)` is the path for the later recovery.

## What needs hardware

1. **Analog.** The zero with VIN off; 0, 2.5, 5 and 15 mA from a bench load on the pogo pins against a meter; the over-current trip into a short.
2. **VREFINT.** Against a meter at 3.4, 3.7 and 4.2 V; decide a per-unit trim if the spread is near ±2.5 %.
3. **Presence.** SWDIO reads high with a powered pod and low with none, through U6 and the 1M pull-down.
4. **HAL248.** Its output type and level with the lid open and shut. Assumed push-pull, high when open; open drain needs a pull-up of at least 4.7M.
5. **SWD.** Clock rate and edges at the pogo pins; DPIDR and CTRL-AP IDR from a pod with APPROTECT on; RESET reboots a hung pod; the pod's current after the reset.
6. **Soft reset and signature.** The pod's /PG pulses on a scope against the 100–400 ms window; the pod reboots on the soft reset and sets flag bit 4 after the signature.
7. **Standby.** About 15 µA with the lid shut (MCU 8–9 µA, DS table 3-8; hall 3 µA); wake on lid, button and USB; the AWU against a stopwatch (LSI tolerance).
8. **LED.** Blue and white brightness at a 3.4 V cell (Vf headroom); colours behind the light pipe.
9. **Thermal.** The NTC against a thermometer; the 45 °C cut on a warm plate.
