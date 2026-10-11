# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(shutterclock_fw LANGUAGES C)

target_sources(app PRIVATE
  src/main.c
  src/camera.c
  src/battery.c
  src/power.c
  src/usb_pm.c
  src/record.c
  src/timekeeping.c
  src/storage.c
  src/ble_adv.c
  src/smp_mgmt.c
  src/config.c
  src/shot_gatt.c
  src/shell_cmds.c
  src/diag.c
  src/wdt_mon.c
)
