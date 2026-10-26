#include "shifting_lights.h"

static ws2812_handleTypeDef *s_ws;
static shift_cfg_t           s_cfg;

static uint8_t s_flash_tick;
static uint8_t s_flash_on;

static void set_all(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0u; i < CFG_SHIFT_LED_COUNT; i++)
        setLedValues(s_ws, CFG_SHIFT_LED_START + i, r, g, b);
}

void shifting_lights_init(ws2812_handleTypeDef *ws2812) {
    s_ws         = ws2812;
    s_flash_tick = 0u;
    s_flash_on   = 0u;
}

void shifting_lights_set_config(const shift_cfg_t *cfg) {
    s_cfg = *cfg;
}

void shifting_lights_update(uint32_t rpm, uint32_t gear) {
    uint32_t rpm_start, rpm_shift;

    if (s_cfg.mode == SHIFT_MODE_GEAR) {
        if (gear == 0u || gear >= SHIFT_GEAR_COUNT) {
            set_all(0u, 0u, 0u);
            return;
        }
        rpm_start = s_cfg.rpm_start_by_gear[gear];
        rpm_shift = s_cfg.rpm_shift_by_gear[gear];
    } else {
        rpm_start = s_cfg.rpm_start;
        rpm_shift = s_cfg.rpm_shift;
    }

    if (rpm >= rpm_shift) {
        if (++s_flash_tick >= s_cfg.flash_ticks) {
            s_flash_tick = 0u;
            s_flash_on   = !s_flash_on;
        }
        if (s_flash_on)
            set_all(s_cfg.flash.r, s_cfg.flash.g, s_cfg.flash.b);
        else
            set_all(0u, 0u, 0u);
        return;
    }

    s_flash_tick = 0u;
    s_flash_on   = 0u;

    uint8_t lit = 0u;
    if (rpm > rpm_start) {
        uint32_t range = rpm_shift - rpm_start;
        uint32_t delta = rpm - rpm_start;
        lit = (uint8_t)((delta * CFG_SHIFT_LED_COUNT + range - 1u) / range);
        if (lit > CFG_SHIFT_LED_COUNT) lit = CFG_SHIFT_LED_COUNT;
    }

    for (uint16_t i = 0u; i < CFG_SHIFT_LED_COUNT; i++) {
        if (i < lit)
            setLedValues(s_ws, CFG_SHIFT_LED_START + i,
                         s_cfg.colors[i].r, s_cfg.colors[i].g, s_cfg.colors[i].b);
        else
            setLedValues(s_ws, CFG_SHIFT_LED_START + i, 0u, 0u, 0u);
    }
}
