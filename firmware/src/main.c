/*
 * ShutterClock firmware.
 *
 * Boots with the camera unpowered and the release lines off, opens a shell
 * on USB CDC ACM, and triggers the camera on the stored schedule (or every
 * CONFIG_SHUTTERCLOCK_AUTO_PERIOD_S seconds on the bench). Based on the
 * SnowGauge firmware (see PROVENANCE.md).
 */

#include <zephyr/kernel.h>
#include <nrfx.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

#include "app.h"
#include "camera.h"
#include "battery.h"
#include "power.h"
#include "usb_pm.h"
#include "record.h"
#include "storage.h"
#include "timekeeping.h"
#include "ble_adv.h"
#include "config.h"
#include "shot_gatt.h"
#include "diag.h"
#include "wdt_mon.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static K_SEM_DEFINE(period_changed, 0, 1);
static uint32_t auto_period_s = CONFIG_SHUTTERCLOCK_AUTO_PERIOD_S;
static bool button_shot_pending;

static void config_changed(void)
{
	k_sem_give(&period_changed);
}

void app_wake_scheduler(void)
{
	k_sem_give(&period_changed);
}

void app_set_auto_period(uint32_t seconds)
{
	auto_period_s = seconds;
	k_sem_give(&period_changed);
}

uint32_t app_get_auto_period(void)
{
	return auto_period_s;
}

static bool first_record_after_boot = true;

int app_shoot_and_store(bool manual, struct shot_result *s_out, struct record *r_out)
{
	struct shot_result s;
	struct record r;
	uint32_t epoch = 0;
	int ret;

	wdt_mon_alive(WDT_CH_MEASURE, WDT_MEASURE_TIMEOUT_S);
	ret = camera_shoot(&s);
	if (s_out) {
		*s_out = s;
	}
	if (ret == -ENOTSUP) {
		LOG_WRN("shot skipped (battery) - stored with the battery voltage");
	} else if (ret) {
		LOG_WRN("shot not detected (%d) - stored anyway", ret);
	}

	record_from_shot(&r, &s);
	if (time_now(&epoch) == 0) {
		r.epoch = epoch;
		r.flags |= (time_get_state() == TIME_SYNCED) ? RECORD_FLAG_TIME_SYNCED
							     : RECORD_FLAG_TIME_ESTIMATED;
	}
	if (manual) {
		r.flags |= RECORD_FLAG_MANUAL;
	}
	if (first_record_after_boot) {
		r.flags |= RECORD_FLAG_FIRST_AFTER_BOOT;
		first_record_after_boot = false;
	}
	r.seq = storage_next_seq();

	int sret = storage_append(&r);

	if (sret) {
		LOG_ERR("record %u not stored (%d)", r.seq, sret);
	}
	if (r_out) {
		*r_out = r;
	}
	(void)ble_adv_update(ret != 0 || sret != 0);
	wdt_mon_disarm(WDT_CH_MEASURE);
	return ret ? ret : sret;
}

static void printk_out(void *ctx, const char *fmt, ...)
{
	ARG_UNUSED(ctx);
	va_list ap;
	char line[160];

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	LOG_INF("%s", line);
}

/* Test-shot button: polled every 100 ms (no GPIO interrupt wiring needed). */
static void button_work_fn(struct k_work *work)
{
	static uint8_t held;

	if (camera_button_pressed()) {
		if (++held == 3) { /* 300 ms debounce / hold */
			button_shot_pending = true;
			k_sem_give(&period_changed);
		}
	} else {
		held = 0;
	}
	k_work_reschedule(k_work_delayable_from_work(work), K_MSEC(100));
}
static K_WORK_DELAYABLE_DEFINE(button_work, button_work_fn);

int main(void)
{
	int ret;

	LOG_INF("ShutterClock FW - board " CONFIG_BOARD_TARGET);
	diag_init();

	ret = battery_init();
	if (ret) {
		LOG_ERR("battery_init: %d", ret);
	}
	ret = power_init();
	if (ret) {
		LOG_ERR("power_init: %d", ret);
	}
	ret = time_init();
	if (ret) {
		LOG_ERR("time_init: %d", ret);
	}
	ret = storage_init();
	if (ret) {
		LOG_ERR("storage_init: %d", ret);
	}
	ret = config_init();
	if (ret) {
		LOG_ERR("config_init: %d", ret);
	}
	ret = camera_init(); /* needs config (always-on mode) */
	if (ret) {
		LOG_ERR("camera_init: %d", ret);
	}
	(void)diag_count_boot(); /* needs the settings subsystem (config_init) */
	(void)time_restore_saved();
	(void)diag_log_boot();
	config_set_change_cb(config_changed);
	ret = ble_adv_init();
	if (ret) {
		LOG_ERR("ble_adv_init: %d", ret);
	}
	ret = shot_gatt_init();
	if (ret) {
		LOG_ERR("shot_gatt_init: %d", ret);
	}
	ret = usb_pm_init();
	if (ret) {
		LOG_ERR("usb_pm_init: %d", ret);
	}
	ret = wdt_mon_init();
	if (ret) {
		LOG_ERR("wdt_mon_init: %d", ret);
	}
	k_work_schedule(&button_work, K_MSEC(500));

	LOG_INF("ready - type 'help' in the USB shell (camera is %s)",
		camera_is_powered() ? "ON" : "OFF");

	bool first = true;

	for (;;) {
		uint32_t period = auto_period_s;

		wdt_mon_alive(WDT_CH_MAIN, WDT_MAIN_TIMEOUT_S);
		if (button_shot_pending) {
			struct shot_result s;
			struct record r;

			button_shot_pending = false;
			LOG_INF("button: test shot");
			led_pulse(50);
			(void)app_shoot_and_store(true, &s, &r);
			shot_print(&s, printk_out, NULL);
			continue;
		}
		if (diag_measure_halted()) {
			/* Reset loop: stay reachable over BLE, do not shoot. */
			k_sem_take(&period_changed, K_MINUTES(10));
			first = true;
			continue;
		}
		if (period != 0) {
			/* Bench mode: fixed period from the shell / Kconfig. */
			if (!first && k_sem_take(&period_changed, K_SECONDS(period)) == 0) {
				first = true;
				continue;
			}
			first = false;
		} else {
			/* Field mode: schedule window from the settings, needs the clock. */
			uint32_t now, next;
			struct app_config c;

			config_get(&c);
			if (c.sched_interval_min == 0) {
				k_sem_take(&period_changed, K_MINUTES(10));
				first = true;
				continue;
			}
			if (time_now(&now) != 0) {
				/*
				 * Clock never set (sync forgotten at installation): still
				 * shoot every interval from boot so nothing is lost; the
				 * records land in rec_notime.bin, ordered by seq.
				 */
				if (!first && k_sem_take(&period_changed, K_MINUTES(c.sched_interval_min)) == 0) {
					first = true;
					continue;
				}
				first = false;
				LOG_WRN("clock unset - shooting on the interval from boot");
			} else if ((next = config_next_trigger(now)) == 0) {
				k_sem_take(&period_changed, K_MINUTES(10));
				first = true;
				continue;
			} else if (next > now) {
				uint32_t wait_s = MIN(next - now, 3600U);

				if (k_sem_take(&period_changed, K_SECONDS(wait_s)) == 0 ||
				    wait_s < next - now) {
					continue; /* config changed, or re-evaluate hourly */
				}
			}
		}

		struct shot_result s;
		struct record r;

		led_pulse(50);
		if (app_shoot_and_store(false, &s, &r) == 0) {
			LOG_INF("record %u stored (%u total)", r.seq, storage_record_count());
		}
		shot_print(&s, printk_out, NULL);
	}
	return 0;
}
