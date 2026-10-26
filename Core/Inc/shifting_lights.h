#ifndef SHIFTING_LIGHTS_H
#define SHIFTING_LIGHTS_H

#include <stdint.h>
#include "ws2812.h"
#include "led_color.h"
#include "config.h"

#define SHIFT_GEAR_COUNT  7u   /* gear table entries: N, 1..6 */

typedef struct {
    uint8_t  mode;                                  /* SHIFT_MODE_FIXED / SHIFT_MODE_GEAR */
    uint32_t rpm_start;                             /* FIXED mode */
    uint32_t rpm_shift;
    uint32_t rpm_start_by_gear[SHIFT_GEAR_COUNT];   /* GEAR mode, index = gear */
    uint32_t rpm_shift_by_gear[SHIFT_GEAR_COUNT];
    rgb_t    colors[CFG_SHIFT_LED_COUNT];           /* index 0 = lowest RPM */
    rgb_t    flash;                                 /* color when rpm >= rpm_shift */
    uint8_t  flash_ticks;                           /* flash half-period, in update calls */
} shift_cfg_t;

void shifting_lights_init(ws2812_handleTypeDef *ws2812);

/* Copies cfg. Call from the same task that calls shifting_lights_update(). */
void shifting_lights_set_config(const shift_cfg_t *cfg);

/* Call every heartbeat tick (~20 ms). gear = current gear number (0 = neutral). */
void shifting_lights_update(uint32_t rpm, uint32_t gear);

#endif /* SHIFTING_LIGHTS_H */
