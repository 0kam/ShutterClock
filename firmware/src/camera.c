/* Camera control - see camera.h */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

#include "camera.h"
#include "config.h"
#include "battery.h"

LOG_MODULE_REGISTER(camera, CONFIG_LOG_DEFAULT_LEVEL);

static const struct gpio_dt_spec cam_en = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), cam_en_gpios);
static const struct gpio_dt_spec ka_en = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), ka_en_gpios);
static const struct gpio_dt_spec af_pin = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), af_gpios);
static const struct gpio_dt_spec shutter_pin =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), shutter_gpios);
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), button_gpios);
static const struct adc_dt_spec adc_cur = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

K_MUTEX_DEFINE(camera_lock);

static bool powered, keepalive;
static int64_t powered_since_ms;
static uint32_t hold_until_s;

static void hold_off_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(hold_off_work, hold_off_work_fn);
static void ka_off_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(ka_off_work, ka_off_work_fn);
static void ka_watch_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(ka_watch_work, ka_watch_work_fn);

int camera_read_ma(uint16_t *ma)
{
	int16_t raw;
	int32_t mv;
	int ret;
	struct adc_sequence seq = {
		.buffer = &raw,
		.buffer_size = sizeof(raw),
	};

	ret = adc_sequence_init_dt(&adc_cur, &seq);
	if (ret == 0) {
		ret = adc_read_dt(&adc_cur, &seq);
	}
	if (ret) {
		return ret;
	}
	mv = raw;
	ret = adc_raw_to_millivolts_dt(&adc_cur, &mv);
	if (ret) {
		return ret;
	}
	if (mv < 0) {
		mv = 0;
	}
	*ma = (uint16_t)MIN(mv * 10, 65535); /* 0.1 ohm: 1 mV = 10 mA */
	return 0;
}

static uint16_t read_ma_or_zero(void)
{
	uint16_t ma = 0;

	(void)camera_read_ma(&ma);
	return ma;
}

static int16_t die_temp_dc(void)
{
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_temp)
	const struct device *dev = DEVICE_DT_GET_ONE(nordic_nrf_temp);
	struct sensor_value v;

	if (device_is_ready(dev) && sensor_sample_fetch(dev) == 0 &&
	    sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &v) == 0) {
		return (int16_t)(v.val1 * 10 + v.val2 / 100000);
	}
#endif
	return INT16_MIN;
}

int camera_power(bool on)
{
	int ret;

	if (on) {
		ret = gpio_pin_set_dt(&cam_en, 1);
		if (ret) {
			return ret;
		}
		if (!powered) {
			powered_since_ms = k_uptime_get();
		}
		powered = true;
		if (keepalive) {
			/* The buck overrides the follower; drop the keep-alive path. */
			(void)gpio_pin_set_dt(&ka_en, 0);
			keepalive = false;
			k_work_cancel_delayable(&ka_off_work);
			k_work_cancel_delayable(&ka_watch_work);
		}
		LOG_DBG("camera power ON");
		return 0;
	}
	ret = gpio_pin_set_dt(&cam_en, 0);
	powered = false;
	LOG_DBG("camera power OFF%s", keepalive ? " (keep-alive stays on)" : "");
	return ret;
}

bool camera_is_powered(void)
{
	return powered;
}

int camera_keepalive(bool on)
{
	if (on) {
		if (!powered) {
			return -EPERM; /* C3 must already sit at 9 V */
		}
		int ret = gpio_pin_set_dt(&ka_en, 1);

		if (ret) {
			return ret;
		}
		keepalive = true;
		k_work_schedule(&ka_watch_work, K_SECONDS(2));
		return 0;
	}
	k_work_cancel_delayable(&ka_off_work);
	k_work_cancel_delayable(&ka_watch_work);
	keepalive = false;
	return gpio_pin_set_dt(&ka_en, 0);
}

bool camera_keepalive_is_on(void)
{
	return keepalive;
}

static void ka_off_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	(void)camera_keepalive(false);
	LOG_INF("keep-alive off");
}

/* Camera woke up on the follower (someone pressed a button): hand it the buck. */
static void ka_watch_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	if (!keepalive || powered) {
		return;
	}
	if (read_ma_or_zero() > CAMERA_KA_ABORT_MA) {
		LOG_WRN("camera awake on keep-alive - buck on");
		(void)camera_power(true);
		(void)camera_hold(2);
		return;
	}
	k_work_schedule(&ka_watch_work, K_SECONDS(2));
}

static void hold_off_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	struct app_config c;

	config_get(&c);
	hold_until_s = 0;
	if (c.cam_mode == CAM_MODE_ALWAYS_ON) {
		return;
	}
	k_mutex_lock(&camera_lock, K_FOREVER);
	(void)camera_power(false);
	k_mutex_unlock(&camera_lock);
	LOG_INF("camera hold ended - power off");
}

int camera_hold(uint32_t minutes)
{
	int ret = 0;

	if (minutes == 0) {
		k_work_cancel_delayable(&hold_off_work);
		hold_until_s = 0;
		struct app_config c;

		config_get(&c);
		if (c.cam_mode != CAM_MODE_ALWAYS_ON) {
			ret = camera_power(false);
		}
		return ret;
	}
	ret = camera_power(true);
	if (ret) {
		return ret;
	}
	hold_until_s = (uint32_t)(k_uptime_get() / 1000) + minutes * 60U;
	k_work_reschedule(&hold_off_work, K_MINUTES(minutes));
	LOG_INF("camera on for %u min", minutes);
	return 0;
}

uint32_t camera_hold_left_s(void)
{
	uint32_t now = (uint32_t)(k_uptime_get() / 1000);

	return hold_until_s > now ? hold_until_s - now : 0;
}

int camera_af(bool on)
{
	if (keepalive && !powered) {
		return -EPERM;
	}
	return gpio_pin_set_dt(&af_pin, on ? 1 : 0);
}

int camera_shutter(bool on)
{
	if (keepalive && !powered) {
		return -EPERM;
	}
	return gpio_pin_set_dt(&shutter_pin, on ? 1 : 0);
}

int camera_release(uint32_t af_ms, uint32_t shutter_ms)
{
	int ret = camera_af(true);

	if (ret) {
		return ret;
	}
	k_sleep(K_MSEC(af_ms));
	ret = camera_shutter(true);
	k_sleep(K_MSEC(shutter_ms));
	(void)camera_shutter(false);
	(void)camera_af(false);
	return ret;
}

bool camera_button_pressed(void)
{
	return gpio_pin_get_dt(&btn) == 1;
}

/*
 * Release once and watch the current: returns the peak (mA) and the time of
 * the peak relative to AF on. Samples every ~2 ms until shutter_ms + 1500 ms.
 */
static uint16_t release_and_watch(uint32_t af_ms, uint32_t shutter_ms, uint16_t *peak_at_ms)
{
	int64_t t0 = k_uptime_get();
	int64_t t_shutter_on = t0 + af_ms;
	int64_t t_shutter_off = t_shutter_on + shutter_ms;
	int64_t t_end = t_shutter_off + 1500;
	bool shutter_on = false, released = false;
	uint16_t peak = 0;

	*peak_at_ms = 0;
	(void)camera_af(true);
	while (k_uptime_get() < t_end) {
		int64_t now = k_uptime_get();
		uint16_t ma = read_ma_or_zero();

		if (ma > peak) {
			peak = ma;
			*peak_at_ms = (uint16_t)MIN(now - t0, 65535);
		}
		if (!shutter_on && now >= t_shutter_on) {
			(void)camera_shutter(true);
			shutter_on = true;
		}
		if (shutter_on && !released && now >= t_shutter_off) {
			(void)camera_shutter(false);
			(void)camera_af(false);
			released = true;
		}
		k_sleep(K_MSEC(2));
	}
	if (!released) {
		(void)camera_shutter(false);
		(void)camera_af(false);
	}
	return peak;
}

int camera_shoot(struct shot_result *r)
{
	struct app_config c;
	int64_t t0, t_af;
	int ret;

	memset(r, 0, sizeof(*r));
	r->t_boot_ms = 0xFFFF;
	r->t_awake_ms = 0xFFFF;
	r->temp_dc = INT16_MIN;
	r->uptime_ms = k_uptime_get();
	config_get(&c);
	r->always_on = (c.cam_mode == CAM_MODE_ALWAYS_ON);
	r->n_shots = MAX(c.cam_n_shots, 1);

	k_mutex_lock(&camera_lock, K_FOREVER);
	(void)battery_read_mv(&r->vbat_mv);
	r->temp_dc = die_temp_dc();
	if (r->vbat_mv != 0 && r->vbat_mv < c.cam_vbat_min_mv) {
		LOG_WRN("vbat %u mV below %u mV - shot skipped", r->vbat_mv, c.cam_vbat_min_mv);
		r->ret = -ENOTSUP;
		k_mutex_unlock(&camera_lock);
		return r->ret;
	}

	bool was_powered = powered;

	t0 = k_uptime_get();
	ret = camera_power(true); /* also drops keep-alive */
	if (ret) {
		r->ret = ret;
		k_mutex_unlock(&camera_lock);
		return ret;
	}
	if (!was_powered) {
		/* Boot: the camera draws > CAMERA_BOOT_MA within ~0.5 s; wait boot_ms anyway. */
		int64_t t_boot_end = t0 + c.cam_boot_ms;

		while (k_uptime_get() < t_boot_end) {
			if (r->t_boot_ms == 0xFFFF && read_ma_or_zero() > CAMERA_BOOT_MA) {
				r->t_boot_ms = (uint16_t)MIN(k_uptime_get() - t0, 65534);
			}
			k_sleep(K_MSEC(20));
		}
	}
	/* Idle current before the release (10 samples). */
	uint32_t acc = 0;

	for (int i = 0; i < 10; i++) {
		acc += read_ma_or_zero();
		k_sleep(K_MSEC(5));
	}
	r->cam_idle_ma = acc / 10;
	(void)battery_read_mv(&r->vbat_load_mv);

	t_af = k_uptime_get();
	for (uint8_t i = 0; i < r->n_shots; i++) {
		uint16_t at;
		uint16_t peak = release_and_watch(c.cam_af_ms, c.cam_shutter_ms, &at);

		if (peak > r->cam_peak_ma) {
			r->cam_peak_ma = peak;
			r->shot_ms = (uint16_t)MIN(k_uptime_get() - t_af - 1500 - c.cam_shutter_ms -
							   c.cam_af_ms + at, 65535);
		}
		if (i + 1 < r->n_shots) {
			k_sleep(K_MSEC(c.cam_shot_gap_ms));
		}
	}
	r->shot_detected = r->cam_peak_ma >= CAMERA_SHOT_MA;

	/* Wait for standby (= card write finished and standby timer expired). */
	int64_t t_timeout = k_uptime_get() + (int64_t)c.cam_write_timeout_s * 1000;
	int below = 0;

	while (k_uptime_get() < t_timeout) {
		if (read_ma_or_zero() < CAMERA_STANDBY_MA) {
			if (++below >= 10) { /* 500 ms continuously below */
				r->write_done = true;
				r->t_awake_ms = (uint16_t)MIN(k_uptime_get() - t_af - 500, 65534);
				break;
			}
		} else {
			below = 0;
		}
		k_sleep(K_MSEC(50));
	}
	if (!r->write_done) {
		LOG_WRN("camera did not return to standby within %u s", c.cam_write_timeout_s);
	}

	if (!r->always_on && camera_hold_left_s() == 0) {
		if (c.cam_ka_min > 0 && r->write_done) {
			ret = camera_keepalive(true);
			if (ret == 0) {
				r->keepalive = true;
				k_work_reschedule(&ka_off_work, K_MINUTES(c.cam_ka_min));
			}
		}
		(void)camera_power(false);
		r->cam_on_ms = (uint16_t)MIN(k_uptime_get() - t0, 65535);
	}
	r->ret = r->shot_detected ? 0 : -EIO;
	k_mutex_unlock(&camera_lock);
	return r->ret;
}

void shot_print(const struct shot_result *r,
		void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	out(ctx, "t=%lld ms  vbat=%u/%u mV (before/awake)  temp=%d.%d C", r->uptime_ms,
	    r->vbat_mv, r->vbat_load_mv, r->temp_dc / 10, abs(r->temp_dc % 10));
	if (r->ret == -ENOTSUP) {
		out(ctx, "skipped: battery below the cutoff");
		return;
	}
	out(ctx, "boot=%u ms  idle=%u mA  peak=%u mA at %u ms  %s", r->t_boot_ms, r->cam_idle_ma,
	    r->cam_peak_ma, r->shot_ms, r->shot_detected ? "SHOT DETECTED" : "no shot detected");
	out(ctx, "awake=%u ms (%s)  on=%u ms  shots=%u  mode=%s%s", r->t_awake_ms,
	    r->write_done ? "standby reached" : "TIMEOUT", r->cam_on_ms, r->n_shots,
	    r->always_on ? "always-on" : "per-shot", r->keepalive ? "  keep-alive on" : "");
}

int camera_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&cam_en) || !gpio_is_ready_dt(&ka_en) || !gpio_is_ready_dt(&af_pin) ||
	    !gpio_is_ready_dt(&shutter_pin) || !gpio_is_ready_dt(&btn)) {
		LOG_ERR("camera GPIOs not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&cam_en, GPIO_OUTPUT_INACTIVE);
	ret |= gpio_pin_configure_dt(&ka_en, GPIO_OUTPUT_INACTIVE);
	ret |= gpio_pin_configure_dt(&af_pin, GPIO_OUTPUT_INACTIVE);
	ret |= gpio_pin_configure_dt(&shutter_pin, GPIO_OUTPUT_INACTIVE);
	ret |= gpio_pin_configure_dt(&btn, GPIO_INPUT);
	if (ret) {
		LOG_ERR("camera GPIO configure failed (%d)", ret);
		return ret;
	}
	if (!adc_is_ready_dt(&adc_cur)) {
		LOG_ERR("ADC not ready");
		return -ENODEV;
	}
	ret = adc_channel_setup_dt(&adc_cur);
	if (ret) {
		LOG_ERR("current ADC channel setup failed (%d)", ret);
		return ret;
	}
	struct app_config c;

	config_get(&c);
	if (c.cam_mode == CAM_MODE_ALWAYS_ON) {
		(void)camera_power(true);
		LOG_INF("camera always-on mode: power ON");
	}
	return 0;
}
