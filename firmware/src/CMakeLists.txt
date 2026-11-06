# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# The board (firmware/boards) and the MAXM86161 binding (firmware/drivers) sit next to src/.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
list(APPEND DTS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(atrium_pod VERSION 0.1.0)

add_subdirectory(../drivers drivers)

target_include_directories(app PRIVATE . power hw ble)
target_sources(app PRIVATE
  main.c
  app_state.c
  activity.c
  log_record.c
  logstore.c
  pulse.c
  hrv.c
  mlc.c
  spo2.c
  dfu_health.c
  prefs.c
  power/charge_policy.c
  power/cutoff.c
  power/dock.c
  power/modes.c
  power/ntc.c
  power/power_sm.c
  power/wear.c
  power/headroom.c
  power/soc_est.c
  hw/power_hw.c
  hw/sensors.c
  hw/status_led.c
  hw/ppg_maxm86161.c
  ble/ble.c
  ble/gatt_atrium.c
  ble/gatt_encode.c
)
