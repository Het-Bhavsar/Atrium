# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(maxm86161 bq21061)

# register logic only; the I2C glue is src/hw/charger.c
target_sources(app PRIVATE bq21061/bq21061_regs.c)

if(CONFIG_MAXM86161)
  target_sources(app PRIVATE maxm86161/maxm86161.c maxm86161/maxm86161_regs.c)
endif()

