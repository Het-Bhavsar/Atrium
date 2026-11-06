# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(maxm86161)

if(CONFIG_MAXM86161)
  target_sources(app PRIVATE maxm86161/maxm86161.c maxm86161/maxm86161_regs.c)
endif()

