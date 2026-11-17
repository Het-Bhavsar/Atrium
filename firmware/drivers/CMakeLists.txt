# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(maxm86161 bq21061 lsm6dsv16bx)

# register logic only; the I2C glue is src/hw/charger.c
target_sources(app PRIVATE bq21061/bq21061_regs.c)

if(CONFIG_MAXM86161)
  target_sources(app PRIVATE maxm86161/maxm86161.c maxm86161/maxm86161_regs.c)
endif()


if(CONFIG_LSM6DSV16BX)
  target_sources(app PRIVATE lsm6dsv16bx/lsm6dsv16bx.c lsm6dsv16bx/lsm6dsv16bx_regs.c)
endif()
