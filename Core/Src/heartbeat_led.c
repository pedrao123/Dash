#include "heartbeat_led.h"
#include "stm32f7xx_hal.h"

// Lub-dub pattern timing (total = 1000 ms @ 1 Hz)
#define PULSE_ON_MS   80u
#define PULSE_GAP_MS  100u
#define PAUSE_MS      740u

static rgb_t                 s_colors[HB_STATUS_COUNT];
static ws2812_handleTypeDef *s_ws;
static uint16_t              s_idx;
static heartbeat_status_t    s_status;

void heartbeat_led_init(ws2812_handleTypeDef *ws2812, uint16_t led_index) {
    s_ws     = ws2812;
    s_idx    = led_index;
    s_status = HB_STATUS_OK;
}

void heartbeat_led_set_colors(const rgb_t colors[HB_STATUS_COUNT]) {
    for (uint8_t i = 0u; i < HB_STATUS_COUNT; i++)
        s_colors[i] = colors[i];
}

void heartbeat_led_set_status(heartbeat_status_t status) {
    s_status = status;
}

void heartbeat_led_tick(void) {
    static uint32_t next_ms = 0;
    static uint8_t  phase   = 0;

    uint32_t now = HAL_GetTick();
    if ((int32_t)(now - next_ms) < 0) return;

    const rgb_t *c = &s_colors[s_status];

    switch (phase) {
        case 0: setLedValues(s_ws, s_idx, c->r, c->g, c->b); next_ms = now + PULSE_ON_MS;  phase = 1; break;
        case 1: setLedValues(s_ws, s_idx, 0, 0, 0);           next_ms = now + PULSE_GAP_MS; phase = 2; break;
        case 2: setLedValues(s_ws, s_idx, c->r, c->g, c->b); next_ms = now + PULSE_ON_MS;  phase = 3; break;
        default: setLedValues(s_ws, s_idx, 0, 0, 0);           next_ms = now + PAUSE_MS;     phase = 0; break;
    }
}
