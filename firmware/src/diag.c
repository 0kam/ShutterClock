/* Boot diagnostics - see diag.h */

#include <zephyr/kernel.h>
#include <zephyr/fatal.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <nrfx.h>
#include <zephyr/settings/settings.h>
#include <stdio.h>
#include <string.h>

#include "diag.h"
#include "storage.h"
#include "timekeeping.h"


LOG_MODULE_REGISTER(diag, CONFIG_LOG_DEFAULT_LEVEL);

#define BOOTS_KEY "sgd/boots"

static struct diag_boot info;
static int64_t holdoff_until_ms;
static uint8_t stored_boots;

static int sgd_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (strcmp(name, "boots") == 0 && len == sizeof(stored_boots)) {
		return read_cb(cb_arg, &stored_boots, len) == (ssize_t)len ? 0 : -EIO;
	}
	return -ENOENT;
}
SETTINGS_STATIC_HANDLER_DEFINE(sgd, "sgd", NULL, sgd_set, NULL, NULL);

static int save_boots(uint8_t n)
{
	int ret = settings_save_one(BOOTS_KEY, &n, sizeof(n));

	if (ret) {
		LOG_ERR("save %s: %d", BOOTS_KEY, ret);
	}
	return ret;
}

/* Clear the consecutive-reset counter once the firmware has run for one hold-off window. */
static void boot_counter_clear(struct k_work *work)
{
	ARG_UNUSED(work);
	(void)save_boots(0);
	LOG_INF("hold-off window passed - reset counter cleared");
}
static K_WORK_DELAYABLE_DEFINE(boot_counter_work, boot_counter_clear);

static enum diag_reset classify(uint32_t cause, enum diag_fatal fatal)
{
	if (cause & RESET_WATCHDOG) {
		return DIAG_RESET_WATCHDOG;
	}
	if (cause & RESET_CPU_LOCKUP) {
		return DIAG_RESET_LOCKUP;
	}
	if (cause & RESET_SOFTWARE) {
		return fatal != DIAG_FATAL_NONE ? DIAG_RESET_FATAL : DIAG_RESET_SOFTWARE;
	}
	if (cause & RESET_PIN) {
		return DIAG_RESET_PIN;
	}
	if (cause == 0) {
		return DIAG_RESET_POWER;
	}
	return DIAG_RESET_OTHER;
}

void diag_init(void)
{
	uint32_t cause = 0;
	uint8_t reg = NRF_POWER->GPREGRET2;

	if (hwinfo_get_reset_cause(&cause) != 0) {
		cause = 0;
	}
	(void)hwinfo_clear_reset_cause();

	info.hw_cause = cause;
	info.fatal = (enum diag_fatal)reg;
	if (info.fatal > DIAG_FATAL_UNKNOWN) {
		info.fatal = DIAG_FATAL_UNKNOWN;
	}
	NRF_POWER->GPREGRET2 = 0; /* fatal reason consumed */
	info.reset = classify(cause, info.fatal);
	info.abnormal = (info.reset == DIAG_RESET_WATCHDOG || info.reset == DIAG_RESET_LOCKUP ||
			 info.reset == DIAG_RESET_FATAL);
	/* Until the counter is loaded: behave like a first boot after a clean start. */
	info.boot_count = 1;
	holdoff_until_ms = 0; /* v2026-10-21: no scheduler hold-off after a reset (user decision) */

	LOG_INF("reset cause: %s (0x%x)%s%s", diag_reset_str(info.reset), cause,
		info.fatal ? " fatal=" : "", info.fatal ? diag_fatal_str(info.fatal) : "");
}

int diag_count_boot(void)
{
	int ret = settings_load_subtree("sgd");

	if (ret) {
		LOG_ERR("load sgd: %d", ret);
	}
	info.boot_count = MIN((uint32_t)stored_boots + 1, 255);
	info.halted = info.boot_count >= CONFIG_SHUTTERCLOCK_BOOT_MAX_RESETS;
	holdoff_until_ms = 0;
	ret = save_boots(info.boot_count);
	k_work_schedule(&boot_counter_work, K_MINUTES(CONFIG_SHUTTERCLOCK_BOOT_HOLDOFF_MIN));

	LOG_INF("boot #%u since the last clean hold-off window", info.boot_count);
	if (info.halted) {
		LOG_ERR("%u consecutive resets - scheduled measurements HALTED until the next reboot",
			info.boot_count);
	} else if (info.boot_count > 1) {
		LOG_WRN("boot #%u within the reset-count window", info.boot_count);
	}
	return ret;
}

/*
 * Persist the cleared counter from the system work queue. Never write flash
 * from a Bluetooth callback: with the SoftDevice Controller a flash write is
 * synchronised with radio timeslots, and blocking the BT RX thread on it
 * stalls the connection set-up (seen 2026-10-21: no app connection, then a
 * watchdog reset).
 */
static bool clear_was_halted;
static uint8_t clear_was_count;

static void boots_clear_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	if (clear_was_count > 1 || clear_was_halted) {
		LOG_INF("BLE connection - reset counter cleared (was %u%s)", clear_was_count,
			clear_was_halted ? ", halted" : "");
		clear_was_count = 0;
		clear_was_halted = false;
	}
	(void)save_boots(0); /* NVS skips the write when the stored value is already 0 */
}
static K_WORK_DEFINE(boots_clear_work, boots_clear_work_fn);

void diag_boots_clear(void)
{
	/* Someone connected over BLE: a person is present, so this is not a reset loop. */
	if (info.boot_count > 1 || info.halted) {
		clear_was_count = info.boot_count;
		clear_was_halted = info.halted;
	}
	info.boot_count = 1;
	info.halted = false;
	/* Safe from any thread / BT callback: only queues the flash write. */
	(void)k_work_submit(&boots_clear_work);
}

const struct diag_boot *diag_boot_info(void)
{
	return &info;
}

bool diag_measure_halted(void)
{
	return info.halted;
}

int64_t diag_holdoff_until_ms(void)
{
	return holdoff_until_ms;
}

const char *diag_reset_str(enum diag_reset r)
{
	switch (r) {
	case DIAG_RESET_POWER:
		return "power/brownout";
	case DIAG_RESET_PIN:
		return "pin";
	case DIAG_RESET_SOFTWARE:
		return "software";
	case DIAG_RESET_WATCHDOG:
		return "watchdog";
	case DIAG_RESET_LOCKUP:
		return "lockup";
	case DIAG_RESET_FATAL:
		return "fatal";
	default:
		return "other";
	}
}

const char *diag_fatal_str(enum diag_fatal f)
{
	switch (f) {
	case DIAG_FATAL_NONE:
		return "none";
	case DIAG_FATAL_CPU_EXCEPTION:
		return "cpu-exception";
	case DIAG_FATAL_SPURIOUS_IRQ:
		return "spurious-irq";
	case DIAG_FATAL_STACK_OVERFLOW:
		return "stack-overflow";
	case DIAG_FATAL_OOPS:
		return "oops";
	case DIAG_FATAL_PANIC:
		return "panic";
	default:
		return "unknown";
	}
}

int diag_log_boot(void)
{
	char line[128];
	char ts[24];
	uint32_t epoch = 0;
	const char *tstate;

	switch (time_get_state()) {
	case TIME_SYNCED:
		tstate = "";
		break;
	case TIME_ESTIMATED:
		tstate = "~";
		break;
	default:
		tstate = "";
		break;
	}
	if (time_now(&epoch) == 0) {
		time_format(epoch, ts, sizeof(ts));
	} else {
		snprintf(ts, sizeof(ts), "unset");
	}
	snprintf(line, sizeof(line), "%s%s boot=%u cause=%s fatal=%s hw=0x%x fw=%s%s\n", tstate,
		 ts, info.boot_count, diag_reset_str(info.reset), diag_fatal_str(info.fatal),
		 info.hw_cause, "ShutterClock", info.halted ? " HALTED" : "");
	return storage_bootlog_append(line);
}

void diag_print(void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	out(ctx, "reset=%s (hw 0x%x)  fatal=%s  boot=%u/%u%s", diag_reset_str(info.reset),
	    info.hw_cause, diag_fatal_str(info.fatal), info.boot_count,
	    CONFIG_SHUTTERCLOCK_BOOT_MAX_RESETS, info.halted ? "  MEASUREMENTS HALTED" : "");
	out(ctx, "hold-off until uptime %lld s (now %lld s)", holdoff_until_ms / 1000,
	    k_uptime_get() / 1000);
}

/*
 * Fatal error handler: reboot instead of Zephyr's default spin (which would
 * only end with the watchdog, and without a trace). The reason survives in
 * GPREGRET2; the kernel has already logged the exception details.
 */
extern void sys_arch_reboot(int type);

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
	ARG_UNUSED(esf);
	uint8_t code;

	switch (reason) {
	case K_ERR_CPU_EXCEPTION:
		code = DIAG_FATAL_CPU_EXCEPTION;
		break;
	case K_ERR_SPURIOUS_IRQ:
		code = DIAG_FATAL_SPURIOUS_IRQ;
		break;
	case K_ERR_STACK_CHK_FAIL:
		code = DIAG_FATAL_STACK_OVERFLOW;
		break;
	case K_ERR_KERNEL_OOPS:
		code = DIAG_FATAL_OOPS;
		break;
	case K_ERR_KERNEL_PANIC:
		code = DIAG_FATAL_PANIC;
		break;
	default:
		/* Architecture-specific codes (ARM: undefined instruction, bus fault ...) */
		code = reason >= K_ERR_ARCH_START ? DIAG_FATAL_CPU_EXCEPTION : DIAG_FATAL_UNKNOWN;
		break;
	}
	NRF_POWER->GPREGRET2 = code;
	LOG_PANIC();
	LOG_ERR("fatal error %u (%s) - resetting", reason, diag_fatal_str(code));
	sys_arch_reboot(0);
	CODE_UNREACHABLE;
}
