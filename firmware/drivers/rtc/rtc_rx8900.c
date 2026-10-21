/*
 * Epson RX8900 SA/CE real-time clock (I2C, DTCXO) - minimal Zephyr RTC driver.
 *
 * Only get/set time are implemented. Register map from the RX8900 application
 * manual ETM45E-11 (tables 7, 8, 37, 58): time/calendar BCD at 00h-06h,
 * Extension 0Dh, Flag 0Eh (VLF bit 1 = oscillation stopped / data invalid,
 * VDET bit 0), Control 0Fh (CSEL bits 7:6, RESET bit 0), Backup 18h.
 *
 * On ShutterClock PCB v1.2 (Akizuki AE-RX8900 module): FOE tied to GND (no
 * FOUT), VBAT from a CR2032 through a series diode, VDD from the XIAO 3V3.
 */
#define DT_DRV_COMPAT epson_rx8900

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <string.h>

LOG_MODULE_REGISTER(rtc_rx8900, CONFIG_RTC_LOG_LEVEL);

#define REG_SEC      0x00
#define REG_EXT      0x0D
#define REG_FLAG     0x0E
#define REG_CTRL     0x0F
#define REG_BACKUP   0x18

#define FLAG_VDET    BIT(0)
#define FLAG_VLF     BIT(1)
#define CTRL_RESET   BIT(0)
#define CTRL_CSEL0   BIT(6) /* temperature compensation every 2 s (default) */

/* Extension: TEST=0, WADA=0 (week alarm), USEL=0, TE=0, FSEL=10 (1 Hz, FOE is low anyway), TSEL=00 */
#define EXT_INIT     0x08
#define CTRL_INIT    CTRL_CSEL0
#define BACKUP_INIT  0x00 /* VDETOFF=0, SWOFF=0, BKSMP=00: switch to VBAT below 2.4 V */

struct rx8900_config {
	struct i2c_dt_spec i2c;
};

/*
 * The "time invalid" state lives in the chip: VLF stays set from the
 * oscillator stop (first power-up, backup lost) until the next set_time, so
 * it survives MCU resets. Never clear VLF anywhere else, or a reboot before
 * the first sync would read the undefined time registers as valid.
 */

/* bcd2bin() / bin2bcd() come from <zephyr/sys/util.h>. */

/* Control / extension / backup registers (undefined after a cold start). Leaves VLF alone. */
static int rx8900_init_regs(const struct device *dev)
{
	const struct rx8900_config *cfg = dev->config;
	int ret;

	ret = i2c_reg_write_byte_dt(&cfg->i2c, REG_EXT, EXT_INIT);
	ret |= i2c_reg_write_byte_dt(&cfg->i2c, REG_CTRL, CTRL_INIT);
	ret |= i2c_reg_write_byte_dt(&cfg->i2c, REG_BACKUP, BACKUP_INIT);
	return ret ? -EIO : 0;
}

static bool rx8900_time_ok(const struct rtc_time *t)
{
	return t->tm_sec >= 0 && t->tm_sec <= 59 && t->tm_min >= 0 && t->tm_min <= 59 &&
	       t->tm_hour >= 0 && t->tm_hour <= 23 && t->tm_mday >= 1 && t->tm_mday <= 31 &&
	       t->tm_mon >= 0 && t->tm_mon <= 11 && t->tm_year >= 100 && t->tm_year <= 199;
}

static int rx8900_get_time(const struct device *dev, struct rtc_time *t)
{
	const struct rx8900_config *cfg = dev->config;
	uint8_t flag, buf[7];
	int ret;

	ret = i2c_reg_read_byte_dt(&cfg->i2c, REG_FLAG, &flag);
	if (ret) {
		return ret;
	}
	if (flag & FLAG_VLF) {
		/* Oscillator stopped since the last set: battery was out or the first power-up. */
		return -ENODATA;
	}
	ret = i2c_burst_read_dt(&cfg->i2c, REG_SEC, buf, sizeof(buf));
	if (ret) {
		return ret;
	}
	memset(t, 0, sizeof(*t));
	t->tm_sec = bcd2bin(buf[0] & 0x7F);
	t->tm_min = bcd2bin(buf[1] & 0x7F);
	t->tm_hour = bcd2bin(buf[2] & 0x3F);
	/* buf[3] = week, one bit per day (bit 0 = Sunday) */
	t->tm_wday = (buf[3] & 0x7F) ? __builtin_ctz(buf[3] & 0x7F) : -1;
	t->tm_mday = bcd2bin(buf[4] & 0x3F);
	t->tm_mon = bcd2bin(buf[5] & 0x1F) - 1;       /* rtc_time: 0..11 */
	t->tm_year = 100 + bcd2bin(buf[6]);          /* rtc_time: years since 1900 */
	t->tm_yday = -1;
	t->tm_isdst = -1;
	t->tm_nsec = 0;
	if (!rx8900_time_ok(t)) {
		LOG_WRN("time registers out of range");
		return -ENODATA;
	}
	return 0;
}

static int rx8900_set_time(const struct device *dev, const struct rtc_time *t)
{
	const struct rx8900_config *cfg = dev->config;
	uint8_t flag, buf[7];
	int ret;

	if (t == NULL || !rx8900_time_ok(t)) {
		return -EINVAL; /* the chip stores 2000..2099 only */
	}
	ret = i2c_reg_read_byte_dt(&cfg->i2c, REG_FLAG, &flag);
	if (ret) {
		return ret;
	}
	if (flag & FLAG_VLF) {
		ret = rx8900_init_regs(dev);
		if (ret) {
			return ret;
		}
	}
	/* RESET stops the sub-second chain; it executes at the STOP of this write and self-clears. */
	ret = i2c_reg_write_byte_dt(&cfg->i2c, REG_CTRL, CTRL_INIT | CTRL_RESET);
	if (ret) {
		return ret;
	}
	buf[0] = bin2bcd(t->tm_sec);
	buf[1] = bin2bcd(t->tm_min);
	buf[2] = bin2bcd(t->tm_hour);
	buf[3] = (t->tm_wday >= 0 && t->tm_wday < 7) ? BIT(t->tm_wday) : BIT(0);
	buf[4] = bin2bcd(t->tm_mday);
	buf[5] = bin2bcd(t->tm_mon + 1);
	buf[6] = bin2bcd(t->tm_year - 100);
	ret = i2c_burst_write_dt(&cfg->i2c, REG_SEC, buf, sizeof(buf));
	if (ret) {
		return ret;
	}
	if (flag & (FLAG_VLF | FLAG_VDET)) {
		/* Only now, with a valid time in the registers, mark the clock valid. */
		ret = i2c_reg_write_byte_dt(&cfg->i2c, REG_FLAG, 0x00);
		if (ret) {
			return ret;
		}
		LOG_INF("VLF cleared - clock valid");
	}
	return 0;
}

static int rx8900_init(const struct device *dev)
{
	const struct rx8900_config *cfg = dev->config;
	uint8_t flag;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		return -ENODEV;
	}
	/* Probe: a board without the module (v1.0 / v1.1) just has no device. */
	ret = i2c_reg_read_byte_dt(&cfg->i2c, REG_FLAG, &flag);
	if (ret) {
		LOG_INF("RX8900 not found (%d)", ret);
		return -ENODEV;
	}
	if (flag & FLAG_VLF) {
		LOG_WRN("VLF set: clock not valid (first power-up or backup lost)");
		/* Registers are undefined after a cold start; VLF stays set until set_time. */
		(void)rx8900_init_regs(dev);
	} else if (flag & FLAG_VDET) {
		LOG_WRN("VDET: VDD was below 1.95 V (backup used), clock kept running");
		(void)i2c_reg_write_byte_dt(&cfg->i2c, REG_FLAG, 0x00);
	}
	LOG_INF("RX8900 ready (flag 0x%02x)", flag);
	return 0;
}

static DEVICE_API(rtc, rx8900_api) = {
	.set_time = rx8900_set_time,
	.get_time = rx8900_get_time,
};

#define RX8900_DEFINE(inst)                                                                   \
	static const struct rx8900_config rx8900_config_##inst = {                            \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                            \
	};                                                                                    \
	DEVICE_DT_INST_DEFINE(inst, rx8900_init, NULL, NULL,                                  \
			      &rx8900_config_##inst, POST_KERNEL, CONFIG_RTC_INIT_PRIORITY,   \
			      &rx8900_api);

DT_INST_FOREACH_STATUS_OKAY(RX8900_DEFINE)
