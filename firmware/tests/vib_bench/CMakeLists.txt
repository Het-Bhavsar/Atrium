# SPDX-License-Identifier: Apache-2.0
# Vibration bench rig for the nRF52 DK: docs/ble_gatt.md 4.5. Build from the repo root:
#   firmware/tests/fw.sh bench
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(atrium_vib_bench)

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_include_directories(app PRIVATE src ${FW}/src ${FW}/src/power)
target_sources(app PRIVATE
  src/main.c
  src/vib_spi.c
  ${FW}/src/vibcap.c
  ${FW}/src/power/modes.c
)
