#include "shifting_lights.h"
#include "config.h"

typedef struct { uint8_t r, g, b; } rgb_t;

static ws2812_handleTypeDef *s_ws;

static const rgb_t s_colors[CFG_SHIFT_LED_COUNT] = CFG_SHIFT_COLORS;

#if CFG_SHIFT_MODE == SHIFT_MODE_GEAR
static const uint32_t s_rpm_start[] = CFG_SHIFT_RPM_START_BY_GEAR;
static const uint32_t s_rpm_shift[] = CFG_SHIFT_RPM_SHIFT_BY_GEAR;
#define GEAR_TABLE_LEN  (sizeof(s_rpm_start) / sizeof(s_rpm_start[0]))
#endif

static uint8_t s_flash_tick;
static uint8_t s_flash_on;

void shifting_lights_init(ws2812_handleTypeDef *ws2812) {
    s_ws         = ws2812;
    s_flash_tick = 0u;
    s_flash_on   = 0u;
}

void shifting_lights_update(uint32_t rpm, uint32_t gear) {
    uint32_t rpm_start, rpm_shift;

#if CFG_SHIFT_MODE == SHIFT_MODE_GEAR
    if (gear == 0u || gear >= GEAR_TABLE_LEN) {
        for (uint16_t i = 0u; i < CFG_SHIFT_LED_COUNT; i++)
            setLedValues(s_ws, CFG_SHIFT_LED_START + i, 0u, 0u, 0u);
        return;
    }
    rpm_start = s_rpm_start[gear];
    rpm_shift = s_rpm_shift[gear];
#else
    (void)gear;
    rpm_start = CFG_SHIFT_RPM_START;
    rpm_shift = CFG_SHIFT_RPM_SHIFT;
#endif

    if (rpm >= rpm_shift) {
        if (++s_flash_tick >= CFG_SHIFT_FLASH_TICKS) {
            s_flash_tick = 0u;
            s_flash_on   = !s_flash_on;
        }
        uint8_t r = s_flash_on ? CFG_SHIFT_FLASH_R : 0u;
        uint8_t g = s_flash_on ? CFG_SHIFT_FLASH_G : 0u;
        uint8_t b = s_flash_on ? CFG_SHIFT_FLASH_B : 0u;
        for (uint16_t i = 0u; i < CFG_SHIFT_LED_COUNT; i++)
            setLedValues(s_ws, CFG_SHIFT_LED_START + i, r, g, b);
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
                         s_colors[i].r, s_colors[i].g, s_colors[i].b);
        else
            setLedValues(s_ws, CFG_SHIFT_LED_START + i, 0u, 0u, 0u);
    }
}
