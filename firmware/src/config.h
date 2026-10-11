/*
 * Persistent configuration (Zephyr settings on the internal storage
 * partition, NVS backend). Readable/writable over SMP settings mgmt from
 * the Web Bluetooth page and the shell ('cfg').
 *
 * Keys (all little-endian integers, sizes as in struct app_config):
 *   sc/sched/start_min       local minutes-of-day the window opens   (u16, 0..1439)
 *   sc/sched/end_min         local minutes-of-day the window closes  (u16; == start = all day,
 *                                                                     < start = spans midnight)
 *   sc/sched/interval_min    minutes between triggers                (u16; 0 = scheduler off)
 *   sc/sched/tz_min          UTC offset of the site in minutes       (i16, e.g. 540 = JST)
 *   sc/cam/mode              0 = power per shot, 1 = always on       (u8)
 *   sc/cam/boot_ms           wait after power on before the release  (u16)
 *   sc/cam/af_ms             AF (half press) before the shutter      (u16)
 *   sc/cam/shutter_ms        SHUTTER (full press) pulse              (u16)
 *   sc/cam/ka_min            keep-alive after each shot, minutes     (u16; 0 = off)
 *   sc/cam/vbat_min_mv       no shot below this battery voltage      (u16)
 *   sc/cam/n_shots           releases per trigger                    (u8, 1..10)
 *   sc/cam/shot_gap_ms       gap between releases                    (u16)
 *   sc/cam/write_timeout_s   max wait for standby after the shot     (u8)
 */
#ifndef SHUTTERCLOCK_CONFIG_H
#define SHUTTERCLOCK_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define CAM_MODE_PER_SHOT  0
#define CAM_MODE_ALWAYS_ON 1

struct app_config {
	uint16_t sched_start_min;
	uint16_t sched_end_min;
	uint16_t sched_interval_min;
	int16_t tz_min;
	uint8_t cam_mode;
	uint16_t cam_boot_ms;
	uint16_t cam_af_ms;
	uint16_t cam_shutter_ms;
	uint16_t cam_ka_min;
	uint16_t cam_vbat_min_mv;
	uint8_t cam_n_shots;
	uint16_t cam_shot_gap_ms;
	uint8_t cam_write_timeout_s;
};

/* Init the settings subsystem and load the stored values. */
int config_init(void);

/* Current values (copy). */
void config_get(struct app_config *out);

/* Replace and persist. Wakes the scheduler. */
int config_set(const struct app_config *cfg);

/* Set one key by its short name ("sched/interval_min") from the shell. */
int config_set_key(const char *name, int32_t value);

/* Print all keys. */
void config_print(void (*out)(void *ctx, const char *fmt, ...), void *ctx);

/*
 * Next scheduled trigger at or after now_epoch (UTC), or 0 when the
 * scheduler is off. start == end means "all day".
 */
uint32_t config_next_trigger(uint32_t now_epoch);

/* Register a callback run (from the caller's context) after any change. */
void config_set_change_cb(void (*cb)(void));

#endif /* SHUTTERCLOCK_CONFIG_H */
