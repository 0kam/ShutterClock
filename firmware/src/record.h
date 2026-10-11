/*
 * ShutterClock shot record, version 1 (one record per trigger).
 *
 * Fixed 40-byte little-endian layout (same size as the SnowGauge record so
 * storage.c is reused unchanged). Decoder: tools/decode_records.py.
 *
 *  off size field
 *    0   2  magic 0x4353 ("SC" little-endian)
 *    2   1  version (1)
 *    3   1  flags (RECORD_FLAG_*)
 *    4   4  epoch (UTC seconds; 0 = unknown)
 *    8   4  seq (monotonic record counter, reset by ERASE)
 *   12   2  vbat_mv (battery before the camera was powered)
 *   14   2  vbat_load_mv (battery while the camera was awake)
 *   16   2  cam_idle_ma (camera awake, before the release)
 *   18   2  cam_peak_ma (highest current during the release; 0 = none)
 *   20   2  t_boot_ms (power on -> camera drawing current; 0xFFFF = was already on)
 *   22   2  shot_ms (AF on -> current peak)
 *   24   2  t_awake_ms (AF on -> back in standby; 0xFFFF = timeout)
 *   26   2  cam_on_ms (buck on time for this trigger; 0 = always-on)
 *   28   2  temp_dc (nRF die temperature, 0.1 degC signed; -32768 = n/a)
 *   30   1  result (signed: 0 ok, -95 battery cutoff, -5 no shot detected, ...)
 *   31   1  n_shots
 *   32   6  reserved (0)
 *   38   2  crc16 (Zephyr crc16_ccitt: poly 0x8408 reflected, init 0xFFFF, bytes 0..37)
 */
#ifndef SHUTTERCLOCK_RECORD_H
#define SHUTTERCLOCK_RECORD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "camera.h"

#define RECORD_SIZE      40
#define RECORD_MAGIC     0x4353U
#define RECORD_VERSION   1

/* flags */
#define RECORD_FLAG_TIME_SYNCED      BIT(0) /* clock set from outside (BLE/shell) since boot */
#define RECORD_FLAG_TIME_ESTIMATED   BIT(1) /* clock restored from the last record at boot */
#define RECORD_FLAG_SHOT_DETECTED    BIT(2) /* current peak above CAMERA_SHOT_MA */
#define RECORD_FLAG_WRITE_DONE       BIT(3) /* camera returned to standby before the timeout */
#define RECORD_FLAG_MANUAL           BIT(4) /* triggered from the shell / BLE / button */
#define RECORD_FLAG_FIRST_AFTER_BOOT BIT(5)
#define RECORD_FLAG_ALWAYS_ON        BIT(6) /* camera power mode: always on */
#define RECORD_FLAG_KEEPALIVE        BIT(7) /* keep-alive path engaged after the shot */

struct record {
	uint8_t flags;
	uint32_t epoch;
	uint32_t seq;
	uint16_t vbat_mv;
	uint16_t vbat_load_mv;
	uint16_t cam_idle_ma;
	uint16_t cam_peak_ma;
	uint16_t t_boot_ms;
	uint16_t shot_ms;
	uint16_t t_awake_ms;
	uint16_t cam_on_ms;
	int16_t temp_dc;
	int8_t result;
	uint8_t n_shots;
};

/* Fill *r from a finished shot. epoch/seq/time flags are set by the caller. */
void record_from_shot(struct record *r, const struct shot_result *s);

/* Serialise to RECORD_SIZE bytes (magic, version and CRC added). */
void record_encode(const struct record *r, uint8_t buf[RECORD_SIZE]);

/* Parse RECORD_SIZE bytes; returns false if magic/version/CRC do not match. */
bool record_decode(const uint8_t buf[RECORD_SIZE], struct record *r);

/* One-line human readable form (CSV-ish) for the shell. */
void record_print(const struct record *r,
		  void (*out)(void *ctx, const char *fmt, ...), void *ctx);

/* Header line matching record_print(). */
void record_print_header(void (*out)(void *ctx, const char *fmt, ...), void *ctx);

#endif /* SHUTTERCLOCK_RECORD_H */
