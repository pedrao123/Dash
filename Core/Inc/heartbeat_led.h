#ifndef HEARTBEAT_LED_H
#define HEARTBEAT_LED_H

#include "ws2812.h"
#include "led_color.h"

typedef enum {
    HB_STATUS_OK    = 0,  // green  — system nominal
    HB_STATUS_WARN  = 1,  // amber  — degraded (e.g. CAN stale)
    HB_STATUS_ERROR = 2,  // red    — critical fault
    HB_STATUS_COUNT
} heartbeat_status_t;

void heartbeat_led_init(ws2812_handleTypeDef *ws2812, uint16_t led_index);
void heartbeat_led_set_colors(const rgb_t colors[HB_STATUS_COUNT]);  // same task as tick()
void heartbeat_led_set_status(heartbeat_status_t status);
void heartbeat_led_tick(void);  // call every ~20 ms from Task_Heartbeat

#endif /* HEARTBEAT_LED_H */
