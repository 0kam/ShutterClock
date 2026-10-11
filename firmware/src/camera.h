/*
 * Camera control: power (CAM_EN -> Q3/Q2 -> buck), keep-alive path (KA_EN ->
 * Q6/Q5 -> follower), wired release through two optocouplers (AF = half
 * press, SHUTTER = full press) and the low-side current sense on A1.
 * Pin map: pcb/README.md. Shot sequence and rules: spec v0.11 section 6.1.
 *
 *  shoot:  [power on -> boot wait] -> AF on -> af_ms -> SHUTTER on -> shutter_ms
 *          -> both off -> wait until the camera is back in standby (current
 *          below CAMERA_STANDBY_MA) or write_timeout -> [keep-alive on ->
 *          power off -> keep-alive off after ka_min] (power-per-shot mode)
 *
 *  Rules enforced here: the release is never driven while only the
 *  keep-alive path is on; keep-alive is entered only from power-on; while
 *  on keep-alive a current above CAMERA_KA_ABORT_MA switches the buck back on.
 */
#ifndef SHUTTERCLOCK_CAMERA_H
#define SHUTTERCLOCK_CAMERA_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

#define CAMERA_STANDBY_MA   50   /* below this the camera is in standby (0.42 mA measured) */
#define CAMERA_SHOT_MA      1000 /* mirror/shutter peak: 3-4 A measured on the D7500 */
#define CAMERA_BOOT_MA      100  /* current rises above this once the camera has booted */
#define CAMERA_KA_ABORT_MA  50   /* camera woke up on keep-alive -> back to the buck */

struct shot_result {
	int64_t uptime_ms;
	uint16_t vbat_mv;        /* before the camera was powered */
	uint16_t vbat_load_mv;   /* while the camera was awake */
	uint16_t cam_idle_ma;    /* awake, before the release */
	uint16_t cam_peak_ma;    /* highest sample during the release window */
	uint16_t t_boot_ms;      /* power on -> current above CAMERA_BOOT_MA (0xFFFF = already on) */
	uint16_t shot_ms;        /* AF on -> peak sample */
	uint16_t t_awake_ms;     /* AF on -> back in standby (0xFFFF = timeout) */
	uint16_t cam_on_ms;      /* buck on time for this shot (0 = always-on mode) */
	int16_t temp_dc;         /* nRF die temperature, 0.1 degC; INT16_MIN = n/a */
	int ret;                 /* 0 ok, -ENOTSUP battery too low (skipped), <0 error */
	bool shot_detected;
	bool write_done;
	bool keepalive;          /* keep-alive engaged after the shot */
	bool always_on;
	uint8_t n_shots;
};

int camera_init(void);

/* Buck path (CAM_EN). Turning on while keep-alive is on also drops keep-alive. */
int camera_power(bool on);
bool camera_is_powered(void);

/* Keep-alive path (KA_EN). Refused (-EPERM) unless the buck is on. */
int camera_keepalive(bool on);
bool camera_keepalive_is_on(void);

/* Raw release lines (bench). Refused (-EPERM) while only keep-alive is on. */
int camera_af(bool on);
int camera_shutter(bool on);

/* AF on -> af_ms -> SHUTTER on -> shutter_ms -> both off. */
int camera_release(uint32_t af_ms, uint32_t shutter_ms);

/* Camera current from the A1 shunt (mA). */
int camera_read_ma(uint16_t *ma);

/* Full sequence according to the stored configuration (config.h). */
int camera_shoot(struct shot_result *r);

/*
 * Installation helper: keep the buck on for `minutes` (0 = off now) so the
 * user can frame the shot; auto-off afterwards (unless always-on mode).
 */
int camera_hold(uint32_t minutes);
uint32_t camera_hold_left_s(void);

/* Test-shot button (D9, active low, internal pull-up). */
bool camera_button_pressed(void);

/* Owner lock: shoot, hold and the GATT live view do not interleave. */
extern struct k_mutex camera_lock;

void shot_print(const struct shot_result *r,
		void (*out)(void *ctx, const char *fmt, ...), void *ctx);

#endif /* SHUTTERCLOCK_CAMERA_H */
