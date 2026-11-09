# SPDX-License-Identifier: Apache-2.0
# Host tests of the vibration bench rig's SPI drain (../src/vib_spi.c) against a simulated
# LSM6DSOX with SPI timing. Run from the repo root: firmware/tests/fw.sh test
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(atrium_vib_bench_tests)

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
target_include_directories(app PRIVATE ../src ${FW}/src ${FW}/src/power)
target_sources(app PRIVATE
  ../src/vib_spi.c
  ${FW}/src/vibcap.c
  ${FW}/src/power/modes.c
  src/test_vib_spi.c
)
