# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# The board (firmware/boards) and the MAXM86161 and BQ21061 bindings (firmware/drivers) sit next to src/.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
list(APPEND DTS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
# One version, in VERSION: Zephyr reads it into APP_VERSION_* and signs the MCUboot header with
# it (MCUBOOT_IMGTOOL_SIGN_VERSION defaults to APP_VERSION_TWEAK_STRING). The DIS Firmware Revision
# below uses the same numbers, so the app's after-update check compares like with like.
project(atrium_pod VERSION ${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_PATCHLEVEL})

add_subdirectory(../drivers drivers)

# DIS Firmware Revision: version + commit (tests/fw.sh passes the commit)
set(ATRIUM_GIT_REV "$ENV{ATRIUM_GIT_REV}")
if(NOT ATRIUM_GIT_REV)
  set(ATRIUM_GIT_REV "unknown")
endif()
target_compile_definitions(app PRIVATE ATRIUM_FW_REV="${APP_VERSION_STRING}+${ATRIUM_GIT_REV}")

# A signing version set by hand (prj.conf, -D) must still match VERSION, or an update would report
# the old number over BLE and the app would call a good update failed.
if(CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION)
  string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION}")
  set(_signed "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
  set(_app "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_PATCHLEVEL}")
  if(NOT _signed STREQUAL _app)
    message(FATAL_ERROR "MCUboot signing version ${CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION} "
      "differs from VERSION (${_app}): bump firmware/src/VERSION instead")
  endif()
endif()

target_include_directories(app PRIVATE . power hw ble)
target_sources(app PRIVATE
  main.c
  app_state.c
  activity.c
  log_record.c
  logpack.c
  logstore.c
  logxfer.c
  epoch.c
  pulse.c
  hrv.c
  mlc.c
  spo2.c
  sigq.c
  vibcap.c
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
  power/vinpat.c
  hw/power_hw.c
  hw/charger.c
  hw/sensors.c
  hw/ppg_maxm86161.c
  ble/ble.c
  ble/gatt_atrium.c
  ble/gatt_encode.c
)
