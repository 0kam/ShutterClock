/* Record encode/decode - see record.h */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "record.h"

void record_from_shot(struct record *r, const struct shot_result *s)
{
	memset(r, 0, sizeof(*r));
	if (s->shot_detected) {
		r->flags |= RECORD_FLAG_SHOT_DETECTED;
	}
	if (s->write_done) {
		r->flags |= RECORD_FLAG_WRITE_DONE;
	}
	if (s->always_on) {
		r->flags |= RECORD_FLAG_ALWAYS_ON;
	}
	if (s->keepalive) {
		r->flags |= RECORD_FLAG_KEEPALIVE;
	}
	r->vbat_mv = s->vbat_mv;
	r->vbat_load_mv = s->vbat_load_mv;
	r->cam_idle_ma = s->cam_idle_ma;
	r->cam_peak_ma = s->cam_peak_ma;
	r->t_boot_ms = s->t_boot_ms;
	r->shot_ms = s->shot_ms;
	r->t_awake_ms = s->t_awake_ms;
	r->cam_on_ms = s->cam_on_ms;
	r->temp_dc = s->temp_dc;
	r->result = (int8_t)CLAMP(s->ret, -127, 127);
	r->n_shots = s->n_shots;
}

void record_encode(const struct record *r, uint8_t buf[RECORD_SIZE])
{
	memset(buf, 0, RECORD_SIZE);
	sys_put_le16(RECORD_MAGIC, &buf[0]);
	buf[2] = RECORD_VERSION;
	buf[3] = r->flags;
	sys_put_le32(r->epoch, &buf[4]);
	sys_put_le32(r->seq, &buf[8]);
	sys_put_le16(r->vbat_mv, &buf[12]);
	sys_put_le16(r->vbat_load_mv, &buf[14]);
	sys_put_le16(r->cam_idle_ma, &buf[16]);
	sys_put_le16(r->cam_peak_ma, &buf[18]);
	sys_put_le16(r->t_boot_ms, &buf[20]);
	sys_put_le16(r->shot_ms, &buf[22]);
	sys_put_le16(r->t_awake_ms, &buf[24]);
	sys_put_le16(r->cam_on_ms, &buf[26]);
	sys_put_le16((uint16_t)r->temp_dc, &buf[28]);
	buf[30] = (uint8_t)r->result;
	buf[31] = r->n_shots;
	sys_put_le16(crc16_ccitt(0xFFFF, buf, RECORD_SIZE - 2), &buf[38]);
}

bool record_decode(const uint8_t buf[RECORD_SIZE], struct record *r)
{
	if (sys_get_le16(&buf[0]) != RECORD_MAGIC || buf[2] != RECORD_VERSION) {
		return false;
	}
	if (sys_get_le16(&buf[38]) != crc16_ccitt(0xFFFF, buf, RECORD_SIZE - 2)) {
		return false;
	}
	r->flags = buf[3];
	r->epoch = sys_get_le32(&buf[4]);
	r->seq = sys_get_le32(&buf[8]);
	r->vbat_mv = sys_get_le16(&buf[12]);
	r->vbat_load_mv = sys_get_le16(&buf[14]);
	r->cam_idle_ma = sys_get_le16(&buf[16]);
	r->cam_peak_ma = sys_get_le16(&buf[18]);
	r->t_boot_ms = sys_get_le16(&buf[20]);
	r->shot_ms = sys_get_le16(&buf[22]);
	r->t_awake_ms = sys_get_le16(&buf[24]);
	r->cam_on_ms = sys_get_le16(&buf[26]);
	r->temp_dc = (int16_t)sys_get_le16(&buf[28]);
	r->result = (int8_t)buf[30];
	r->n_shots = buf[31];
	return true;
}

void record_print_header(void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	out(ctx, "seq,epoch,flags,vbat_mv,vbat_load_mv,idle_ma,peak_ma,boot_ms,shot_ms,awake_ms,"
		 "on_ms,temp_c,result,n_shots");
}

void record_print(const struct record *r,
		  void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	char t[12];

	if (r->temp_dc == INT16_MIN) {
		t[0] = '\0';
	} else {
		snprintf(t, sizeof(t), "%s%d.%d", r->temp_dc < 0 ? "-" : "", abs(r->temp_dc) / 10,
			 abs(r->temp_dc) % 10);
	}
	out(ctx, "%u,%u,0x%02x,%u,%u,%u,%u,%u,%u,%u,%u,%s,%d,%u", r->seq, r->epoch, r->flags,
	    r->vbat_mv, r->vbat_load_mv, r->cam_idle_ma, r->cam_peak_ma, r->t_boot_ms, r->shot_ms,
	    r->t_awake_ms, r->cam_on_ms, t, r->result, r->n_shots);
}
