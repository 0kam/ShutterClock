/*
 * Shot / installation GATT service (replaces SnowGauge's calibration service).
 *
 * Service  53430001-6E63-4D61-8B0C-7A8E9F0B1C2D
 *  live    53430002-...  notify, 8 bytes LE every ~250 ms while enabled:
 *                        vbat_mv u16, cam_ma u16, state u8 (SHOT_STATE_*),
 *                        hold_left_s u16, reserved u8
 *  control 53430003-...  write: 0x00 live off, 0x01 live on,
 *                        0x10 TEST SHOT (full sequence, stored as a MANUAL record),
 *                        0x11 + u16 minutes: camera on for N minutes (0 = off now),
 *                        0x12 AF only (half press for 2 s; camera must be on),
 *                        0x20 'E' 'R' 'A' 'S' 'E' erase all records
 *  status  53430004-...  read, 16 bytes LE: state u8, last_cmd_result i8,
 *                        busy u8, cmd_seq u8, vbat_mv u16, cam_ma u16,
 *                        last_peak_ma u16, last_awake_ms u16, hold_left_s u16,
 *                        record_count u16 (saturating)
 *
 * Writes while busy are rejected (ATT "procedure already in progress").
 * ERASE is refused (-EACCES) unless the record files were downloaded since
 * boot (fs mgmt read of rec_*.bin) or there are no records.
 */
#ifndef SHUTTERCLOCK_SHOT_GATT_H
#define SHUTTERCLOCK_SHOT_GATT_H

#include <stdint.h>
#include <stdbool.h>

#define SHOT_LIVE_TIMEOUT_S 300

#define SHOT_STATE_CAM_POWERED  BIT(0)
#define SHOT_STATE_KEEPALIVE    BIT(1)
#define SHOT_STATE_LIVE         BIT(2)
#define SHOT_STATE_ALWAYS_ON    BIT(3)

int shot_gatt_init(void);
bool shot_live_is_on(void);
void shot_live_stop(void);

/* fs mgmt hook: a record file was read since boot (unlocks ERASE). */
void shot_note_records_downloaded(void);

#endif /* SHUTTERCLOCK_SHOT_GATT_H */
