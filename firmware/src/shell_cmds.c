/*
 * Shell commands for bench testing (USB CDC ACM console).
 *
 *   cam on|off|status              camera power (buck path, CAM_EN)
 *   cam hold <min>                 camera on for N minutes, then off (0 = off now)
 *   ka on|off                      keep-alive path (KA_EN); on needs the buck on
 *   af on|off                      half-press line (raw)
 *   shutter on|off                 full-press line (raw)
 *   release [af_ms] [shutter_ms]   AF -> SHUTTER pulse with the current camera state
 *   cur [n] [ms]                   camera current, n samples every ms
 *   batt                           battery voltage
 *   shoot                          full sequence per cfg, stored as a record (flag MANUAL)
 *   auto [s]                       automatic trigger period (0 = off)
 *   cfg show|sched|set             settings (sc/sched/..., sc/cam/...)
 *   rec count|ls|dump [n]|hex [n]|erase ERASE   record storage
 *   dfu | reboot                   bootloader (serial DFU) / warm reset
 *   time [set <epoch>]             wall clock (UTC epoch seconds)
 *   ble [adv <min_ms> <max_ms>]    advertising status / interval (max 0 = stop)
 *   diag [log]                     reset cause, boot counter, fatal reason; boot.log
 *   wdt [stall main|shot|ble]      watchdog supervisor status; simulate a stuck subsystem
 *   hang | fault | panic           deliberately lock up / crash (watchdog + diag tests)
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include <nrfx.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "camera.h"
#include "battery.h"
#include "app.h"
#include "record.h"
#include "storage.h"
#include "timekeeping.h"
#include "ble_adv.h"
#include "config.h"
#include "shot_gatt.h"
#include "diag.h"
#include "wdt_mon.h"

static void shell_out(void *ctx, const char *fmt, ...)
{
	const struct shell *sh = ctx;
	va_list ap;
	char line[200];

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	shell_print(sh, "%s", line);
}

static bool arg_on(const char *s)
{
	return strcmp(s, "on") == 0 || strcmp(s, "1") == 0;
}

/* ---- cam ---- */

static int cmd_cam_status(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t ma = 0, mv = 0;

	(void)camera_read_ma(&ma);
	(void)battery_read_mv(&mv);
	shell_print(sh, "camera power %s  keep-alive %s  hold left %u s  current %u mA  vbat %u mV",
		    camera_is_powered() ? "ON" : "OFF", camera_keepalive_is_on() ? "ON" : "off",
		    camera_hold_left_s(), ma, mv);
	return 0;
}

static int cmd_cam_power(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	k_mutex_lock(&camera_lock, K_FOREVER);
	ret = arg_on(argv[0]) ? camera_power(true) : camera_hold(0);
	k_mutex_unlock(&camera_lock);
	if (ret) {
		shell_error(sh, "failed (%d)", ret);
		return ret;
	}
	return cmd_cam_status(sh, 1, argv);
}

static int cmd_cam_hold(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t min = strtoul(argv[1], NULL, 0);
	int ret;

	k_mutex_lock(&camera_lock, K_FOREVER);
	ret = camera_hold(min);
	k_mutex_unlock(&camera_lock);
	if (ret) {
		shell_error(sh, "failed (%d)", ret);
		return ret;
	}
	return cmd_cam_status(sh, 1, argv);
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cam,
	SHELL_CMD(on, NULL, "Camera power on (CAM_EN high)", cmd_cam_power),
	SHELL_CMD(off, NULL, "Camera power off", cmd_cam_power),
	SHELL_CMD_ARG(hold, NULL, "Camera on for N minutes: hold <min> (0 = off now)", cmd_cam_hold, 2, 0),
	SHELL_CMD(status, NULL, "Camera power / keep-alive / current / battery", cmd_cam_status),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(cam, &sub_cam, "Camera power", NULL);

/* ---- ka / af / shutter / release ---- */

static int cmd_ka(const struct shell *sh, size_t argc, char **argv)
{
	int ret = camera_keepalive(arg_on(argv[1]));

	if (ret == -EPERM) {
		shell_error(sh, "keep-alive needs the buck on first ('cam on')");
		return ret;
	}
	if (ret) {
		shell_error(sh, "failed (%d)", ret);
		return ret;
	}
	shell_print(sh, "keep-alive %s", camera_keepalive_is_on() ? "ON" : "off");
	return 0;
}
SHELL_CMD_ARG_REGISTER(ka, NULL, "Keep-alive path: ka on|off", cmd_ka, 2, 0);

static int cmd_af(const struct shell *sh, size_t argc, char **argv)
{
	int ret = camera_af(arg_on(argv[1]));

	if (ret) {
		shell_error(sh, "failed (%d)", ret);
		return ret;
	}
	shell_print(sh, "AF line %s", arg_on(argv[1]) ? "ON" : "off");
	return 0;
}
SHELL_CMD_ARG_REGISTER(af, NULL, "Half-press line: af on|off", cmd_af, 2, 0);

static int cmd_shutter(const struct shell *sh, size_t argc, char **argv)
{
	int ret = camera_shutter(arg_on(argv[1]));

	if (ret) {
		shell_error(sh, "failed (%d)", ret);
		return ret;
	}
	shell_print(sh, "SHUTTER line %s", arg_on(argv[1]) ? "ON" : "off");
	return 0;
}
SHELL_CMD_ARG_REGISTER(shutter, NULL, "Full-press line: shutter on|off", cmd_shutter, 2, 0);

static int cmd_release(const struct shell *sh, size_t argc, char **argv)
{
	struct app_config c;
	int ret;

	config_get(&c);
	uint32_t af_ms = argc > 1 ? strtoul(argv[1], NULL, 0) : c.cam_af_ms;
	uint32_t sh_ms = argc > 2 ? strtoul(argv[2], NULL, 0) : c.cam_shutter_ms;

	k_mutex_lock(&camera_lock, K_FOREVER);
	ret = camera_release(af_ms, sh_ms);
	k_mutex_unlock(&camera_lock);
	if (ret) {
		shell_error(sh, "release failed (%d)", ret);
		return ret;
	}
	shell_print(sh, "released: AF %u ms then SHUTTER %u ms", af_ms, sh_ms);
	return 0;
}
SHELL_CMD_ARG_REGISTER(release, NULL, "AF -> SHUTTER pulse: release [af_ms] [shutter_ms]",
		       cmd_release, 1, 2);

/* ---- cur / batt ---- */

static int cmd_cur(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t n = argc > 1 ? strtoul(argv[1], NULL, 0) : 1;
	uint32_t ms = argc > 2 ? strtoul(argv[2], NULL, 0) : 100;
	uint16_t ma, peak = 0;
	uint32_t sum = 0;

	for (uint32_t i = 0; i < n; i++) {
		int ret = camera_read_ma(&ma);

		if (ret) {
			shell_error(sh, "read failed (%d)", ret);
			return ret;
		}
		sum += ma;
		peak = MAX(peak, ma);
		if (n <= 20) {
			shell_print(sh, "%u mA", ma);
		}
		if (i + 1 < n) {
			k_sleep(K_MSEC(ms));
		}
	}
	if (n > 1) {
		shell_print(sh, "n=%u mean=%u mA peak=%u mA", n, sum / n, peak);
	}
	return 0;
}
SHELL_CMD_ARG_REGISTER(cur, NULL, "Camera current: cur [n] [ms]", cmd_cur, 1, 2);

static int cmd_batt(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t mv = 0;
	int ret = battery_read_mv(&mv);

	if (ret) {
		shell_error(sh, "battery read failed (%d)", ret);
		return ret;
	}
	shell_print(sh, "vbat = %u mV (A0 x11, trim %d permille)", mv,
		    CONFIG_SHUTTERCLOCK_VBAT_GAIN_PERMILLE);
	return 0;
}
SHELL_CMD_REGISTER(batt, NULL, "Battery voltage", cmd_batt);

/* ---- dfu / reboot ---- */

#define ADAFRUIT_DFU_MAGIC_SERIAL_ONLY_RESET 0x4E

static int cmd_dfu(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "rebooting into bootloader (serial DFU) ...");
	(void)camera_keepalive(false);
	(void)camera_power(false);
	k_sleep(K_MSEC(200));
	NRF_POWER->GPREGRET = ADAFRUIT_DFU_MAGIC_SERIAL_ONLY_RESET;
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}
SHELL_CMD_REGISTER(dfu, NULL, "Reboot into the bootloader for serial DFU", cmd_dfu);

static int cmd_reboot(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "rebooting ...");
	(void)camera_keepalive(false);
	(void)camera_power(false);
	k_sleep(K_MSEC(100));
	sys_reboot(SYS_REBOOT_WARM);
	return 0;
}
SHELL_CMD_REGISTER(reboot, NULL, "Warm reset into the application", cmd_reboot);

/* ---- diag / wdt / crash tests ---- */

static int cmd_diag(const struct shell *sh, size_t argc, char **argv)
{
	diag_print(shell_out, (void *)sh);
	if (argc > 1 && strcmp(argv[1], "log") == 0) {
		int ret = storage_bootlog_print(shell_out, (void *)sh);

		if (ret) {
			shell_error(sh, "boot.log: %d", ret);
		}
	}
	return 0;
}
SHELL_CMD_ARG_REGISTER(diag, NULL, "Boot diagnostics: diag [log]", cmd_diag, 1, 1);

static int cmd_wdt(const struct shell *sh, size_t argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "stall") == 0) {
		static const char *const names[WDT_CH_COUNT] = { "main", "shot", "ble" };

		for (int i = 0; i < WDT_CH_COUNT; i++) {
			if (strcmp(argv[2], names[i]) == 0) {
				wdt_mon_stall((enum wdt_ch)i);
				shell_print(sh, "channel %s stalled - expect a watchdog reset", names[i]);
				return 0;
			}
		}
		shell_error(sh, "unknown channel (main|shot|ble)");
		return -EINVAL;
	}
	wdt_mon_status(shell_out, (void *)sh);
	return 0;
}
SHELL_CMD_ARG_REGISTER(wdt, NULL, "Watchdog: wdt [stall main|shot|ble]", cmd_wdt, 1, 2);

static int cmd_hang(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "locking interrupts and spinning - watchdog reset in <= %u s",
		    CONFIG_SHUTTERCLOCK_WDT_TIMEOUT_S);
	k_sleep(K_MSEC(100));
	(void)irq_lock();
	for (;;) {
	}
	return 0;
}
SHELL_CMD_REGISTER(hang, NULL, "TEST: lock up the CPU (watchdog reset)", cmd_hang);

static int cmd_fault(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "raising a CPU exception (undefined instruction)");
	k_sleep(K_MSEC(100));
	__asm__ volatile("udf #0");
	return 0;
}
SHELL_CMD_REGISTER(fault, NULL, "TEST: CPU exception -> fatal handler -> reboot", cmd_fault);

static int cmd_panic(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "k_panic()");
	k_sleep(K_MSEC(100));
	k_panic();
	return 0;
}
SHELL_CMD_REGISTER(panic, NULL, "TEST: kernel panic -> fatal handler -> reboot", cmd_panic);

/* ---- shoot / auto ---- */

static int cmd_shoot(const struct shell *sh, size_t argc, char **argv)
{
	struct shot_result s;
	struct record r;
	int ret = app_shoot_and_store(true, &s, &r);

	shot_print(&s, shell_out, (void *)sh);
	shell_print(sh, "stored as record %u (%u total)", r.seq, storage_record_count());
	record_print_header(shell_out, (void *)sh);
	record_print(&r, shell_out, (void *)sh);
	if (ret) {
		shell_warn(sh, "result %d", ret);
	}
	return ret;
}
SHELL_CMD_REGISTER(shoot, NULL, "One full shot sequence, stored as a record", cmd_shoot);

static int cmd_auto(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		app_set_auto_period(strtoul(argv[1], NULL, 0));
	}
	uint32_t s = app_get_auto_period();

	if (s) {
		shell_print(sh, "auto trigger every %u s", s);
	} else {
		shell_print(sh, "auto trigger off (schedule from cfg applies)");
	}
	return 0;
}
SHELL_CMD_ARG_REGISTER(auto, NULL, "Auto trigger period: auto [s] (0 = off)", cmd_auto, 1, 1);

/* ---- rec: record storage ---- */

static int cmd_rec_count(const struct shell *sh, size_t argc, char **argv)
{
	struct record last;
	char ts[24];

	shell_print(sh, "records=%u  next seq=%u  fs=%s  mirror=%u/%u slots",
		    storage_record_count(), storage_next_seq(),
		    storage_fs_ok() ? "mounted" : "NOT mounted",
		    storage_mirror_count(), storage_mirror_capacity());
	if (storage_last_record(&last) == 0) {
		time_format(last.epoch, ts, sizeof(ts));
		shell_print(sh, "last: seq=%u %s peak=%u mA vbat=%u mV flags=0x%02x result=%d",
			    last.seq, ts, last.cam_peak_ma, last.vbat_mv, last.flags, last.result);
	}
	return 0;
}

static int cmd_rec_ls(const struct shell *sh, size_t argc, char **argv)
{
	int ret = storage_list_files(shell_out, (void *)sh);

	if (ret) {
		shell_error(sh, "list failed (%d)", ret);
	}
	return ret;
}

static int cmd_rec_dump(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t n = 10;
	uint32_t total = storage_mirror_count();
	struct record r;

	if (argc > 1) {
		n = strtoul(argv[1], NULL, 0);
	}
	if (n == 0 || n > total) {
		n = total;
	}
	record_print_header(shell_out, (void *)sh);
	for (uint32_t i = total - n; i < total; i++) {
		int ret = storage_mirror_read(i, &r);

		if (ret == 0) {
			record_print(&r, shell_out, (void *)sh);
		} else {
			shell_print(sh, "# slot %u: bad (%d)", i, ret);
		}
	}
	return 0;
}

static int cmd_rec_hex(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t n = 1;
	uint32_t total = storage_mirror_count();
	struct record r;
	uint8_t buf[RECORD_SIZE];
	char line[2 * RECORD_SIZE + 1];

	if (argc > 1) {
		n = strtoul(argv[1], NULL, 0);
	}
	if (n == 0 || n > total) {
		n = total;
	}
	for (uint32_t i = total - n; i < total; i++) {
		if (storage_mirror_read(i, &r) != 0) {
			shell_print(sh, "# slot %u: bad", i);
			continue;
		}
		record_encode(&r, buf);
		for (int b = 0; b < RECORD_SIZE; b++) {
			snprintf(&line[2 * b], 3, "%02x", buf[b]);
		}
		shell_print(sh, "%s", line);
	}
	return 0;
}

static int cmd_rec_erase(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	if (argc < 2 || strcmp(argv[1], "ERASE") != 0) {
		shell_error(sh, "refusing: type 'rec erase ERASE' to delete all records");
		return -EINVAL;
	}
	ret = storage_erase_all();
	shell_print(sh, ret ? "erase failed (%d)" : "all records erased", ret);
	return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_rec,
	SHELL_CMD(count, NULL, "Record count and storage state", cmd_rec_count),
	SHELL_CMD(ls, NULL, "List record files on LittleFS", cmd_rec_ls),
	SHELL_CMD_ARG(dump, NULL, "Print the newest n records (CSV): dump [n=10, 0=all]",
		      cmd_rec_dump, 1, 1),
	SHELL_CMD_ARG(hex, NULL, "Newest n records as raw hex (for tools/decode_records.py --hex)",
		      cmd_rec_hex, 1, 1),
	SHELL_CMD_ARG(erase, NULL, "Delete all records: erase ERASE", cmd_rec_erase, 1, 1),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(rec, &sub_rec, "Record storage", NULL);

/* ---- time ---- */

static const char *time_state_str(enum time_state s)
{
	switch (s) {
	case TIME_SYNCED:
		return "synced";
	case TIME_ESTIMATED:
		return "estimated (restored from last record)";
	default:
		return "unset";
	}
}

static int cmd_time(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t epoch = 0;
	char ts[24];

	if (argc == 2 || (argc > 2 && strcmp(argv[1], "set") != 0)) {
		shell_error(sh, "usage: time [set <epoch>]");
		return -EINVAL;
	}
	if (argc > 2) {
		epoch = strtoul(argv[2], NULL, 0);
		if (epoch < 1700000000U) {
			shell_error(sh, "epoch looks wrong (want UTC seconds, e.g. 1780000000)");
			return -EINVAL;
		}
		int ret = time_set(epoch, true);

		if (ret) {
			shell_error(sh, "time set failed (%d)", ret);
			return ret;
		}
	}
	if (time_now(&epoch) == 0) {
		time_format(epoch, ts, sizeof(ts));
		shell_print(sh, "%s (%u) - %s", ts, epoch, time_state_str(time_get_state()));
	} else {
		shell_print(sh, "time unset - 'time set <epoch>' (UTC seconds)");
	}
	return 0;
}
SHELL_CMD_ARG_REGISTER(time, NULL, "Wall clock: time [set <epoch>]", cmd_time, 1, 2);

/* ---- ble ---- */

static int cmd_ble(const struct shell *sh, size_t argc, char **argv)
{
	if (argc >= 2 && strcmp(argv[1], "adv") == 0) {
		uint32_t min_ms = 0, max_ms = 0;

		if (argc == 3) {
			min_ms = max_ms = strtoul(argv[2], NULL, 0);
		} else if (argc == 4) {
			min_ms = strtoul(argv[2], NULL, 0);
			max_ms = strtoul(argv[3], NULL, 0);
		} else {
			shell_error(sh, "usage: ble adv <ms> | ble adv <min_ms> <max_ms> | ble adv 0");
			return -EINVAL;
		}
		int ret = ble_adv_set_interval(min_ms, max_ms);

		if (ret) {
			shell_error(sh, "adv interval failed (%d)", ret);
			return ret;
		}
	} else if (argc != 1) {
		shell_error(sh, "usage: ble [adv ...]");
		return -EINVAL;
	}
	ble_adv_status(shell_out, (void *)sh);
	shell_print(sh, "live mode %s", shot_live_is_on() ? "on" : "off");
	return 0;
}
SHELL_CMD_ARG_REGISTER(ble, NULL, "BLE: ble [adv <min_ms> [max_ms]]", cmd_ble, 1, 3);

/* ---- cfg: settings ---- */

static int cmd_cfg_show(const struct shell *sh, size_t argc, char **argv)
{
	struct app_config c;
	uint32_t now = 0, next;
	char ts[24];

	config_get(&c);
	shell_print(sh, "schedule: %02u:%02u-%02u:%02u local, every %u min, tz %+d min%s",
		    c.sched_start_min / 60, c.sched_start_min % 60, c.sched_end_min / 60,
		    c.sched_end_min % 60, c.sched_interval_min, c.tz_min,
		    c.sched_interval_min ? "" : " (OFF)");
	config_print(shell_out, (void *)sh);
	if (time_now(&now) == 0 && (next = config_next_trigger(now)) != 0) {
		time_format(next, ts, sizeof(ts));
		shell_print(sh, "next scheduled shot: %s (in %u s)", ts, next - now);
	} else {
		shell_print(sh, "next scheduled shot: none (clock unset or scheduler off)");
	}
	if (app_get_auto_period()) {
		shell_print(sh, "bench auto period %u s overrides the schedule", app_get_auto_period());
	}
	return 0;
}

static int parse_hhmm(const char *s, uint16_t *min)
{
	unsigned int h, m;

	if (sscanf(s, "%u:%u", &h, &m) != 2 || h > 23 || m > 59) {
		return -EINVAL;
	}
	*min = (uint16_t)(h * 60 + m);
	return 0;
}

static int cmd_cfg_sched(const struct shell *sh, size_t argc, char **argv)
{
	struct app_config c;

	config_get(&c);
	if (parse_hhmm(argv[1], &c.sched_start_min) || parse_hhmm(argv[2], &c.sched_end_min)) {
		shell_error(sh, "usage: cfg sched <HH:MM> <HH:MM> <interval_min> [tz_min]");
		return -EINVAL;
	}
	c.sched_interval_min = (uint16_t)strtoul(argv[3], NULL, 0);
	if (argc > 4) {
		c.tz_min = (int16_t)strtol(argv[4], NULL, 0);
	}
	int ret = config_set(&c);

	if (ret) {
		shell_error(sh, "config save failed (%d)", ret);
		return ret;
	}
	return cmd_cfg_show(sh, 1, argv);
}

static int cmd_cfg_set(const struct shell *sh, size_t argc, char **argv)
{
	int ret = config_set_key(argv[1], (int32_t)strtol(argv[2], NULL, 0));

	if (ret == -ENOENT) {
		shell_error(sh, "unknown key (see 'cfg show'; e.g. cam/af_ms)");
		return ret;
	}
	if (ret) {
		shell_error(sh, "rejected (%d)", ret);
		return ret;
	}
	return cmd_cfg_show(sh, 1, argv);
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cfg,
	SHELL_CMD(show, NULL, "Show all settings and the next shot", cmd_cfg_show),
	SHELL_CMD_ARG(sched, NULL, "Set schedule: sched <HH:MM> <HH:MM> <interval_min> [tz_min]",
		      cmd_cfg_sched, 4, 1),
	SHELL_CMD_ARG(set, NULL, "Set one key: set <key> <value> (e.g. cam/mode 1)", cmd_cfg_set, 3, 0),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(cfg, &sub_cfg, "Configuration (settings)", NULL);
