# Firmware

nRF52832 firmware for the pod, built on the nRF5 SDK / Zephyr [VERIFY choice at bring-up]. Not started.

| Folder | Contents |
|---|---|
| `src/` | Application: sampling scheduler, BLE service, flash log, power modes |
| `drivers/` | MAX86174A, LSM6DSV16X, TMP117, MAX17048, W25Q64JW, LPB1010 ship mode |
| `boards/` | Board definition: pin map, clocks, DC/DC off (LDO mode) |
| `tests/` | Host-side unit tests and bench scripts |

Licence: Apache-2.0 (see `LICENSE`).
