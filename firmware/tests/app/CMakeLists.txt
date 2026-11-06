# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(atrium_app_tests)

# Reference vectors from app/python/make_test_vectors.py (atrium_ml, Atrium synthetic signals;
# the mlc_config program also carries ST's BSD-3 notice, see src/mlc.c)
set(VEC ${CMAKE_CURRENT_SOURCE_DIR}/vectors)
foreach(case rest_64bpm walk_72bpm_steps108 run_140bpm_steps168 hrv_rest_58bpm_rmssd45 offbody_noise)
  foreach(kind input hr hrv rmssd)
    generate_inc_file_for_target(app ${VEC}/${case}_${kind}.csv
      ${ZEPHYR_BINARY_DIR}/include/generated/vectors/${case}_${kind}.inc)
  endforeach()
endforeach()
foreach(case still head_motion walk run)
  foreach(kind input levels class)
    generate_inc_file_for_target(app ${VEC}/mlc_${case}_${kind}.csv
      ${ZEPHYR_BINARY_DIR}/include/generated/vectors/mlc_${case}_${kind}.inc)
  endforeach()
endforeach()
generate_inc_file_for_target(app ${VEC}/mlc_config.csv
  ${ZEPHYR_BINARY_DIR}/include/generated/vectors/mlc_config.inc)
foreach(case r045 r070 moved low_perfusion)
  foreach(kind input windows burst)
    generate_inc_file_for_target(app ${VEC}/spo2_${case}_${kind}.csv
      ${ZEPHYR_BINARY_DIR}/include/generated/vectors/spo2_${case}_${kind}.inc)
  endforeach()
endforeach()

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_include_directories(app PRIVATE ${FW}/src ${FW}/src/power ${FW}/src/ble ${FW}/drivers/maxm86161)
target_sources(app PRIVATE
  ${FW}/src/activity.c
  ${FW}/src/log_record.c
  ${FW}/src/pulse.c
  ${FW}/src/hrv.c
  ${FW}/src/mlc.c
  ${FW}/src/spo2.c
  ${FW}/src/dfu_health.c
  ${FW}/src/ble/gatt_encode.c
  ${FW}/drivers/maxm86161/maxm86161_regs.c
  src/test_activity.c
  src/test_gatt_encode.c
  src/test_hrv.c
  src/test_ml_vectors.c
  src/test_dfu_health.c
  src/test_log_record.c
  src/test_maxm86161.c
  src/test_mlc.c
  src/test_spo2.c
  src/test_pulse.c
)
