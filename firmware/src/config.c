/* Persistent configuration - see config.h */

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"

LOG_MODULE_REGISTER(config, CONFIG_LOG_DEFAULT_LEVEL);

static struct app_config cfg = {
	.sched_start_min = CONFIG_SHUTTERCLOCK_SCHED_DEFAULT_START_MIN,
	.sched_end_min = CONFIG_SHUTTERCLOCK_SCHED_DEFAULT_END_MIN,
	.sched_interval_min = CONFIG_SHUTTERCLOCK_SCHED_DEFAULT_INTERVAL_MIN,
	.tz_min = CONFIG_SHUTTERCLOCK_SCHED_DEFAULT_TZ_MIN,
	.cam_mode = CAM_MODE_PER_SHOT,
	.cam_boot_ms = 2500,
	.cam_af_ms = 300,
	.cam_shutter_ms = 200,
	.cam_ka_min = 5,
	.cam_vbat_min_mv = 11500,
	.cam_n_shots = 1,
	.cam_shot_gap_ms = 1000,
	.cam_write_timeout_s = 20,
};
static K_MUTEX_DEFINE(lock);
static void (*change_cb)(void);

struct key_map {
	const char *name;
	void *ptr;
	size_t size;
	bool is_signed;
};

#define KEY(n, f, s) { n, &cfg.f, sizeof(cfg.f), s }
static const struct key_map keys[] = {
	KEY("sched/start_min", sched_start_min, false),
	KEY("sched/end_min", sched_end_min, false),
	KEY("sched/interval_min", sched_interval_min, false),
	KEY("sched/tz_min", tz_min, true),
	KEY("cam/mode", cam_mode, false),
	KEY("cam/boot_ms", cam_boot_ms, false),
	KEY("cam/af_ms", cam_af_ms, false),
	KEY("cam/shutter_ms", cam_shutter_ms, false),
	KEY("cam/ka_min", cam_ka_min, false),
	KEY("cam/vbat_min_mv", cam_vbat_min_mv, false),
	KEY("cam/n_shots", cam_n_shots, false),
	KEY("cam/shot_gap_ms", cam_shot_gap_ms, false),
	KEY("cam/write_timeout_s", cam_write_timeout_s, false),
};

static const struct key_map *find_key(const char *name)
{
	for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
		if (strcmp(keys[i].name, name) == 0) {
			return &keys[i];
		}
	}
	return NULL;
}

static bool validate(void)
{
	return cfg.sched_start_min < 1440 && cfg.sched_end_min < 1440 &&
	       cfg.sched_interval_min <= 1440 && cfg.tz_min >= -14 * 60 && cfg.tz_min <= 14 * 60 &&
	       cfg.cam_mode <= CAM_MODE_ALWAYS_ON && cfg.cam_boot_ms <= 30000 &&
	       cfg.cam_af_ms <= 10000 && cfg.cam_shutter_ms >= 20 && cfg.cam_shutter_ms <= 10000 &&
	       cfg.cam_ka_min <= 1440 && cfg.cam_n_shots >= 1 && cfg.cam_n_shots <= 10 &&
	       cfg.cam_shot_gap_ms <= 60000 && cfg.cam_write_timeout_s >= 1;
}

static int sc_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	const struct key_map *k = find_key(name);
	uint8_t tmp[8];

	if (!k) {
		return -ENOENT;
	}
	if (len != k->size) {
		LOG_WRN("setting %s: bad length %u (want %u)", name, (unsigned int)len,
			(unsigned int)k->size);
		return -EINVAL;
	}
	if (read_cb(cb_arg, tmp, len) != (ssize_t)len) {
		return -EIO;
	}
	k_mutex_lock(&lock, K_FOREVER);
	struct app_config saved = cfg;

	memcpy(k->ptr, tmp, len);
	if (!validate()) {
		cfg = saved;
		k_mutex_unlock(&lock);
		LOG_WRN("setting %s: value out of range, rejected", name);
		return -EINVAL;
	}
	k_mutex_unlock(&lock);
	if (change_cb) {
		change_cb();
	}
	return 0;
}

static int sc_get(const char *name, char *val, int val_len_max)
{
	const struct key_map *k = find_key(name);

	if (!k) {
		return -ENOENT;
	}
	if (val_len_max < (int)k->size) {
		return -ENOMEM;
	}
	k_mutex_lock(&lock, K_FOREVER);
	memcpy(val, k->ptr, k->size);
	k_mutex_unlock(&lock);
	return (int)k->size;
}

static int sc_export(int (*cb)(const char *name, const void *value, size_t val_len))
{
	char name[40];

	for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
		snprintk(name, sizeof(name), "sc/%s", keys[i].name);
		(void)cb(name, keys[i].ptr, keys[i].size);
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(sc, "sc", sc_get, sc_set, NULL, sc_export);

static int32_t key_value(const struct key_map *k)
{
	switch (k->size) {
	case 1:
		return *(uint8_t *)k->ptr;
	case 2:
		return k->is_signed ? *(int16_t *)k->ptr : *(uint16_t *)k->ptr;
	default:
		return (int32_t)*(uint32_t *)k->ptr;
	}
}

void config_print(void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	k_mutex_lock(&lock, K_FOREVER);
	for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
		out(ctx, "sc/%-20s = %d", keys[i].name, key_value(&keys[i]));
	}
	k_mutex_unlock(&lock);
}

int config_init(void)
{
	int ret = settings_subsys_init();

	if (ret) {
		LOG_ERR("settings init: %d", ret);
		return ret;
	}
	ret = settings_load_subtree("sc");
	if (ret) {
		LOG_ERR("settings load: %d", ret);
		return ret;
	}
	LOG_INF("schedule %02u:%02u-%02u:%02u every %u min (tz %+d min); camera mode %u, "
		"boot %u ms, af %u ms, shutter %u ms, keep-alive %u min, cutoff %u mV",
		cfg.sched_start_min / 60, cfg.sched_start_min % 60, cfg.sched_end_min / 60,
		cfg.sched_end_min % 60, cfg.sched_interval_min, cfg.tz_min, cfg.cam_mode,
		cfg.cam_boot_ms, cfg.cam_af_ms, cfg.cam_shutter_ms, cfg.cam_ka_min,
		cfg.cam_vbat_min_mv);
	return 0;
}

void config_get(struct app_config *out)
{
	k_mutex_lock(&lock, K_FOREVER);
	*out = cfg;
	k_mutex_unlock(&lock);
}

int config_set(const struct app_config *in)
{
	int ret;

	k_mutex_lock(&lock, K_FOREVER);
	struct app_config saved = cfg;

	cfg = *in;
	if (!validate()) {
		cfg = saved;
		k_mutex_unlock(&lock);
		return -EINVAL;
	}
	k_mutex_unlock(&lock);
	ret = settings_save_subtree("sc");
	if (change_cb) {
		change_cb();
	}
	return ret;
}

int config_set_key(const char *name, int32_t value)
{
	const struct key_map *k = find_key(name);
	char full[40];
	int ret;

	if (!k) {
		return -ENOENT;
	}
	k_mutex_lock(&lock, K_FOREVER);
	struct app_config saved = cfg;

	switch (k->size) {
	case 1:
		*(uint8_t *)k->ptr = (uint8_t)value;
		break;
	case 2:
		*(uint16_t *)k->ptr = (uint16_t)value;
		break;
	default:
		*(uint32_t *)k->ptr = (uint32_t)value;
		break;
	}
	if (!validate()) {
		cfg = saved;
		k_mutex_unlock(&lock);
		return -EINVAL;
	}
	k_mutex_unlock(&lock);
	/*
	 * settings_save_subtree() matches the *handler* name ("sc"), so a deeper
	 * subtree silently saves nothing: save the one key explicitly.
	 */
	snprintk(full, sizeof(full), "sc/%s", k->name);
	ret = settings_save_one(full, k->ptr, k->size);
	if (change_cb) {
		change_cb();
	}
	return ret;
}

uint32_t config_next_trigger(uint32_t now_epoch)
{
	struct app_config c;

	config_get(&c);
	if (c.sched_interval_min == 0) {
		return 0;
	}

	uint32_t iv = (uint32_t)c.sched_interval_min * 60U;
	int64_t local = (int64_t)now_epoch + (int64_t)c.tz_min * 60;
	int64_t day0 = local - (local % 86400); /* local midnight, as a local timestamp */
	uint32_t start = (uint32_t)c.sched_start_min * 60U;
	uint32_t end = (uint32_t)c.sched_end_min * 60U;

	/*
	 * Candidate windows: the one that opened today and, if it spans
	 * midnight, the one that opened yesterday; pick the earliest slot >= now.
	 */
	for (int d = -1; d <= 1; d++) {
		int64_t w_start = day0 + d * 86400 + start;
		int64_t w_end;

		if (start == end) {
			w_end = w_start + 86400; /* all day */
		} else if (end > start) {
			w_end = day0 + d * 86400 + end;
		} else {
			w_end = day0 + (d + 1) * 86400 + end; /* spans midnight */
		}
		int64_t slot;

		if (local < w_start) {
			slot = w_start;
		} else {
			slot = w_start + ((local - w_start + iv - 1) / iv) * iv;
		}
		if (slot < w_end || (slot == w_end && start != end)) {
			return (uint32_t)(slot - (int64_t)c.tz_min * 60);
		}
	}
	return 0; /* cannot happen: the d = +1 window always yields a slot */
}

void config_set_change_cb(void (*cb)(void))
{
	change_cb = cb;
}
