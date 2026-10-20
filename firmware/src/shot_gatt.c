/* Shot / installation GATT service - see shot_gatt.h */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "shot_gatt.h"
#include "app.h"
#include "config.h"
#include "camera.h"
#include "battery.h"
#include "storage.h"
#include "ble_adv.h"

LOG_MODULE_REGISTER(shot_gatt, CONFIG_LOG_DEFAULT_LEVEL);

#define SC_UUID(n) BT_UUID_128_ENCODE(0x53430000 + (n), 0x6E63, 0x4D61, 0x8B0C, 0x7A8E9F0B1C2D)
static const struct bt_uuid_128 uuid_svc = BT_UUID_INIT_128(SC_UUID(1));
static const struct bt_uuid_128 uuid_live = BT_UUID_INIT_128(SC_UUID(2));
static const struct bt_uuid_128 uuid_ctrl = BT_UUID_INIT_128(SC_UUID(3));
static const struct bt_uuid_128 uuid_status = BT_UUID_INIT_128(SC_UUID(4));

#define CMD_LIVE_OFF  0x00
#define CMD_LIVE_ON   0x01
#define CMD_TEST_SHOT 0x10
#define CMD_CAM_HOLD  0x11
#define CMD_AF_ONLY   0x12
#define CMD_ERASE     0x20

static bool live_on;
static bool live_notify_enabled;
static int8_t last_result;
static bool cmd_busy;
static uint8_t cmd_seq;
static bool records_downloaded;
static int64_t live_started_ms;
static uint8_t live_buf[8];
static uint16_t last_peak_ma, last_awake_ms;

K_THREAD_STACK_DEFINE(shot_wq_stack, 2048);
static struct k_work_q shot_wq;

static void live_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(live_work, live_work_fn);
static void cmd_work_fn(struct k_work *work);
static K_WORK_DEFINE(cmd_work, cmd_work_fn);
static uint8_t pending_cmd;
static uint16_t pending_arg;
static bool hold_from_app;

static void app_hold_off_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	if (!hold_from_app) {
		return;
	}
	hold_from_app = false;
	k_mutex_lock(&camera_lock, K_FOREVER);
	if (camera_hold_left_s() > 0) {
		LOG_WRN("BLE gone while the camera was held on by the app - power off");
		(void)camera_hold(0);
	}
	k_mutex_unlock(&camera_lock);
}
static K_WORK_DEFINE(app_hold_off_work, app_hold_off_work_fn);

void shot_gatt_on_disconnect(void)
{
	if (hold_from_app) {
		k_work_submit_to_queue(&shot_wq, &app_hold_off_work);
	}
}

static uint8_t state_byte(void)
{
	struct app_config c;
	uint8_t s = 0;

	config_get(&c);
	if (camera_is_powered()) {
		s |= SHOT_STATE_CAM_POWERED;
	}
	if (camera_keepalive_is_on()) {
		s |= SHOT_STATE_KEEPALIVE;
	}
	if (live_on) {
		s |= SHOT_STATE_LIVE;
	}
	if (c.cam_mode == CAM_MODE_ALWAYS_ON) {
		s |= SHOT_STATE_ALWAYS_ON;
	}
	return s;
}

static ssize_t read_status(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	uint8_t st[16];
	uint16_t vbat = 0, ma = 0;

	(void)battery_read_mv(&vbat);
	(void)camera_read_ma(&ma);
	st[0] = state_byte();
	st[1] = (uint8_t)last_result;
	st[2] = cmd_busy ? 1 : 0;
	st[3] = cmd_seq;
	sys_put_le16(vbat, &st[4]);
	sys_put_le16(ma, &st[6]);
	sys_put_le16(last_peak_ma, &st[8]);
	sys_put_le16(last_awake_ms, &st[10]);
	sys_put_le16((uint16_t)MIN(camera_hold_left_s(), 65535), &st[12]);
	sys_put_le16((uint16_t)MIN(storage_record_count(), 65535), &st[14]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, st, sizeof(st));
}

static ssize_t write_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
			  uint16_t len, uint16_t offset, uint8_t flags)
{
	const uint8_t *d = buf;

	if (offset != 0 || len < 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if (cmd_busy) {
		return BT_GATT_ERR(BT_ATT_ERR_PROCEDURE_IN_PROGRESS);
	}
	pending_arg = 0;
	switch (d[0]) {
	case CMD_LIVE_OFF:
	case CMD_LIVE_ON:
	case CMD_TEST_SHOT:
	case CMD_AF_ONLY:
		pending_cmd = d[0];
		break;
	case CMD_CAM_HOLD:
		if (len != 3) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		pending_cmd = d[0];
		pending_arg = sys_get_le16(&d[1]);
		break;
	case CMD_ERASE:
		if (len != 6 || memcmp(&d[1], "ERASE", 5) != 0) {
			last_result = -EACCES;
			return BT_GATT_ERR(BT_ATT_ERR_AUTHORIZATION);
		}
		if (!records_downloaded && storage_record_count() > 0) {
			LOG_WRN("ERASE refused: records not downloaded since boot");
			last_result = -EACCES;
			return BT_GATT_ERR(BT_ATT_ERR_AUTHORIZATION);
		}
		pending_cmd = d[0];
		break;
	default:
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	cmd_busy = true;
	cmd_seq++;
	last_result = 0;
	k_work_submit_to_queue(&shot_wq, &cmd_work);
	return len;
}

static void live_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	live_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(shot_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_svc),
	BT_GATT_CHARACTERISTIC(&uuid_live.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL,
			       live_buf),
	BT_GATT_CCC(live_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(&uuid_ctrl.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL,
			       write_ctrl, NULL),
	BT_GATT_CHARACTERISTIC(&uuid_status.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_status, NULL, NULL),
);

#define LIVE_ATTR (&shot_svc.attrs[2])

static void live_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	uint16_t vbat = 0, ma = 0;

	if (!live_on) {
		return;
	}
	if (k_uptime_get() - live_started_ms > SHOT_LIVE_TIMEOUT_S * 1000LL) {
		LOG_WRN("live mode timeout");
		shot_live_stop();
		return;
	}
	(void)battery_read_mv(&vbat);
	(void)camera_read_ma(&ma);
	sys_put_le16(vbat, &live_buf[0]);
	sys_put_le16(ma, &live_buf[2]);
	live_buf[4] = state_byte();
	sys_put_le16((uint16_t)MIN(camera_hold_left_s(), 65535), &live_buf[5]);
	live_buf[7] = 0;
	if (live_notify_enabled) {
		(void)bt_gatt_notify(NULL, LIVE_ATTR, live_buf, sizeof(live_buf));
	}
	k_work_schedule_for_queue(&shot_wq, &live_work, K_MSEC(250));
}

static int live_start(void)
{
	if (live_on) {
		return 0;
	}
	live_on = true;
	live_started_ms = k_uptime_get();
	k_work_schedule_for_queue(&shot_wq, &live_work, K_MSEC(100));
	LOG_INF("live mode on");
	return 0;
}

void shot_live_stop(void)
{
	if (!live_on) {
		return;
	}
	live_on = false;
	k_work_cancel_delayable(&live_work);
	LOG_INF("live mode off");
}

bool shot_live_is_on(void)
{
	return live_on;
}

static void cmd_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	int ret = 0;

	switch (pending_cmd) {
	case CMD_LIVE_ON:
		ret = live_start();
		break;
	case CMD_LIVE_OFF:
		shot_live_stop();
		break;
	case CMD_TEST_SHOT: {
		struct shot_result s;
		struct record r;

		ret = app_shoot_and_store(true, &s, &r);
		last_peak_ma = s.cam_peak_ma;
		last_awake_ms = s.t_awake_ms;
		break;
	}
	case CMD_CAM_HOLD:
		k_mutex_lock(&camera_lock, K_FOREVER);
		ret = camera_hold(pending_arg);
		k_mutex_unlock(&camera_lock);
		hold_from_app = (ret == 0 && pending_arg > 0);
		break;
	case CMD_AF_ONLY:
		k_mutex_lock(&camera_lock, K_FOREVER);
		if (!camera_is_powered()) {
			ret = -EPERM;
		} else {
			ret = camera_af(true);
			k_sleep(K_SECONDS(2));
			(void)camera_af(false);
		}
		k_mutex_unlock(&camera_lock);
		break;
	case CMD_ERASE:
		ret = storage_erase_all();
		(void)ble_adv_update(false);
		LOG_WRN("ERASE over BLE: %d", ret);
		break;
	default:
		ret = -EINVAL;
	}
	last_result = (int8_t)CLAMP(ret, -127, 127);
	cmd_busy = false;
}

void shot_note_records_downloaded(void)
{
	records_downloaded = true;
}

int shot_gatt_init(void)
{
	k_work_queue_start(&shot_wq, shot_wq_stack, K_THREAD_STACK_SIZEOF(shot_wq_stack),
			   K_PRIO_PREEMPT(10), NULL);
	k_thread_name_set(&shot_wq.thread, "shot_wq");
	return 0;
}
