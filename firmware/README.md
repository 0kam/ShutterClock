# ShutterClock firmware

Zephyr / nRF Connect SDK application for the Seeed XIAO nRF52840 (non-Sense) on
ShutterClock PCB v1.0. Based on the SnowGauge firmware (`PROVENANCE.md`).

- SDK: **nRF Connect SDK v3.4.0** (`nrfutil sdk-manager`), board target `xiao_ble/nrf52840`
- Pin map source of truth: [`pcb/README.md`](../pcb/README.md)
- Status: **builds (2026-10-11, FLASH 58 %), not yet run on hardware** (boards and parts in transit)

| Module | File | Purpose |
|---|---|---|
| Camera | `src/camera.c` | CAM_EN (buck) / KA_EN (keep-alive follower) / AF / SHUTTER optocouplers, A1 current sense; `camera_shoot()` = power on → boot wait → AF → SHUTTER → wait for standby → keep-alive → power off |
| Battery | `src/battery.c` | A0 ungated x11 divider (trim `CONFIG_SHUTTERCLOCK_VBAT_GAIN_PERMILLE`) |
| Record | `src/record.c` | 40-byte shot record v1 (magic "SC"), layout in `record.h` |
| Storage | `src/storage.c` | LittleFS `/lfs1/rec_YYYYMM.bin` + internal-flash mirror (SnowGauge, unchanged) |
| Clock | `src/timekeeping.c` | emulated RTC, UNSET / ESTIMATED / SYNCED, NVS checkpoint (unchanged) |
| Config | `src/config.c` | settings `sc/sched/*` (window + interval + tz) and `sc/cam/*` (mode, boot_ms, af_ms, shutter_ms, ka_min, vbat_min_mv, n_shots, shot_gap_ms, write_timeout_s) |
| BLE adv | `src/ble_adv.c` | name `SC-XXXX`, Manufacturer Data v3 (vbat, record count, last peak mA, flags, epoch, uptime, boot count, reset cause) |
| SMP | `src/smp_mgmt.c` | mcumgr os/fs/settings over BLE and USB shell (unchanged) |
| Shot GATT | `src/shot_gatt.c` | service `53430001-…`: live (vbat/current/state), control (test shot, camera hold N min, AF only, ERASE), status |
| Diag / WDT / USB PM / power | unchanged from SnowGauge | reset-loop protection, 120 s watchdog, USB only with VBUS, QSPI runtime PM |
| Shell | `src/shell_cmds.c` | `cam on|off|hold|status`, `ka`, `af`, `shutter`, `release`, `cur`, `batt`, `shoot`, `auto`, `cfg show|sched|set`, `rec …`, `time`, `ble`, `diag`, `wdt`, `dfu`, `reboot` |

## Build

```bash
cd /opt/nordic/ncs/v3.4.0 && nrfutil sdk-manager toolchain launch --ncs-version v3.4.0 -- west build -d "$REPO/firmware/build" -b xiao_ble/nrf52840 "$REPO/firmware"
```

Add `-p` for a pristine build. Output: `firmware/build/firmware/zephyr/zephyr.uf2` / `zephyr.hex` (app linked at 0x27000, Adafruit bootloader kept).

## Flash

Same as SnowGauge (`docs/03_firmware.md` there): UF2 drag-and-drop, or on macOS 26 serial DFU with `adafruit-nrfutil` and a package built with `--sd-req 0xFFFE`. **Remove JP1 before connecting USB** when the board is on battery power.

## First bench session (to do)

1. `cam status`, `batt` (compare with a DMM, set the trim), `cur` with the camera off (expect ~0 mA)
2. `cam on` → the camera boots; `cur 50 100` → idle ~220–300 mA; `release` → shutter fires; `cur` peak > 1 A
3. `shoot` → full sequence; check `rec dump`; tune `cfg set cam/af_ms` / `cam/boot_ms`
4. `ka on` with the camera on, then `cam off` → camera stays in standby on the follower (`cur` ≈ 0.4 mA)
5. Sleep current with picowatt (target ≈ 50 µA whole board)
