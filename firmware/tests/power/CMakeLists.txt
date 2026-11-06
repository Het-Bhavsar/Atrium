# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(atrium_power_tests)

set(FW_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/power)
target_include_directories(app PRIVATE ${FW_SRC})
target_sources(app PRIVATE
  ${FW_SRC}/charge_policy.c
  ${FW_SRC}/cutoff.c
  ${FW_SRC}/dock.c
  ${FW_SRC}/modes.c
  ${FW_SRC}/ntc.c
  ${FW_SRC}/power_sm.c
  ${FW_SRC}/wear.c
  ${FW_SRC}/headroom.c
  ${FW_SRC}/soc_est.c
  src/test_charge_policy.c
  src/test_cutoff.c
  src/test_dock.c
  src/test_modes.c
  src/test_ntc.c
  src/test_power_sm.c
  src/test_wear.c
  src/test_headroom.c
  src/test_soc_est.c
)
