/*
 * Battery voltage on A0: VIN through an ungated 1 M / 100 k divider
 * (x11), 100 nF at the node. SAADC gain 1/6, 40 us acquisition.
 */
#ifndef SHUTTERCLOCK_BATTERY_H
#define SHUTTERCLOCK_BATTERY_H

#include <stdint.h>

int battery_init(void);

/* Battery voltage in mV (0 on error). */
int battery_read_mv(uint16_t *vbat_mv);

#endif /* SHUTTERCLOCK_BATTERY_H */
