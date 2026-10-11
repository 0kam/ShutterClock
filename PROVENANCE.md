# Provenance

Files copied from [SnowGauge](https://github.com/0kam/SnowGauge).
Fork point: commit `36e21f8` (FW `fw-2026-09-07b`).

When a bug is fixed on either side, check this table and port the fix by hand.
Add a row whenever a file is copied; update "Changes" when it diverges.

| ShutterClock path | SnowGauge source path | SnowGauge commit | Changes |
|---|---|---|---|
| `pcb/lib/XIAO-nRF52840-DIP.kicad_mod` | `pcb/lib/XIAO-nRF52840-DIP.kicad_mod` | `36e21f8` (file unchanged since `b0c9c5e`) | none — same XIAO footprint (14 through-hole pads, 15.24 mm rows) |
| `pcb/generate_board.py` (structure) | `pcb/generate_board.py` | `36e21f8` | rewritten for the ShutterClock netlist; same pcbnew approach (nets → footprints → tracks → outline → GND pour → silk) |
| `firmware/src/timekeeping.c/.h` | `firmware/src/timekeeping.c/.h` | `36e21f8` | unchanged except CONFIG_SNOWGAUGE_ -> CONFIG_SHUTTERCLOCK_ |
| `firmware/src/storage.c/.h` | `firmware/src/storage.c/.h` | `36e21f8` | unchanged (record size stays 40 B; magic differs) |
| `firmware/src/diag.c/.h` | `firmware/src/diag.c/.h` | `36e21f8` | lidar_name() -> "ShutterClock" in the boot log line; Kconfig prefix |
| `firmware/src/wdt_mon.c/.h` | `firmware/src/wdt_mon.c/.h` | `36e21f8` | channel name "measure" -> "shot"; Kconfig prefix |
| `firmware/src/usb_pm.c/.h` | `firmware/src/usb_pm.c/.h` | `36e21f8` | unchanged |
| `firmware/src/power.c/.h` | `firmware/src/power.c/.h` | `36e21f8` | unchanged |
| `firmware/src/smp_mgmt.c` | `firmware/src/smp_mgmt.c` | `36e21f8` | cal_note_records_downloaded -> shot_note_records_downloaded |
| `firmware/src/ble_adv.c/.h` | `firmware/src/ble_adv.c/.h` | `36e21f8` | name SC-XXXX, payload v3 (cam_peak_ma instead of distance, LAST_SHOT_OK flag) |
| `firmware/src/config.c/.h` | `firmware/src/config.c/.h` | `36e21f8` | rewritten: sc/sched/* + sc/cam/* keys, config_set_key(), config_next_trigger() |
| `firmware/src/record.c/.h` | `firmware/src/record.c/.h` | `36e21f8` | rewritten: shot record v1 (magic "SC") |
| `firmware/src/battery.c/.h` | `firmware/src/battery.c/.h` | `36e21f8` | rewritten: x11 ungated divider, trim Kconfig |
| `firmware/src/shot_gatt.c/.h` | `firmware/src/cal_gatt.c/.h` | `36e21f8` | rewritten from cal_gatt.c: test shot / hold / AF / ERASE / live |
| `firmware/src/main.c` | `firmware/src/main.c` | `36e21f8` | scheduler loop kept; measure -> camera_shoot; button polling |
| `firmware/src/shell_cmds.c` | `firmware/src/shell_cmds.c` | `36e21f8` | rewritten: cam/ka/af/shutter/release/cur/shoot/cfg; rec/time/ble/diag/wdt/dfu kept |
| `firmware/prj.conf` | `firmware/prj.conf` | `36e21f8` | UART/I2C/IMU options removed, TEMP_NRF5 added, names changed |
| `firmware/Kconfig` | `firmware/Kconfig` | `36e21f8` | rewritten (SHUTTERCLOCK_* options) |
| `firmware/boards/xiao_ble_nrf52840.overlay` | `firmware/boards/xiao_ble_nrf52840_sense.overlay` | `36e21f8` | from xiao_ble_nrf52840_sense.overlay: new GPIO/ADC map, no IMU, temp sensor; flash/fs/rtc layout unchanged |
| `firmware/src/camera.c/.h` | (new; replaces sensor_rail.c + measure.c) | — | camera power / keep-alive / release / current sense |
