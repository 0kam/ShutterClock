/* Application-level state shared between main, the shell and the GATT service. */
#ifndef SHUTTERCLOCK_APP_H
#define SHUTTERCLOCK_APP_H

#include <stdint.h>
#include <stdbool.h>

#include "camera.h"
#include "record.h"

void app_set_auto_period(uint32_t seconds);
void app_wake_scheduler(void);
uint32_t app_get_auto_period(void);

/*
 * Run one shot sequence and store it as a record (LittleFS + mirror).
 * manual = true marks shell/BLE/button-triggered records. *s / *r may be NULL.
 * Returns the storage result (0 = stored) or the shot error.
 */
int app_shoot_and_store(bool manual, struct shot_result *s, struct record *r);

#endif /* SHUTTERCLOCK_APP_H */
