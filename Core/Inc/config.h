#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>

static inline uint32_t cfg_ect_convert(int16_t raw) {
    int32_t c = ((int32_t)raw - 320) * 5 / 90;
    return (uint32_t)(c < 0 ? 0 : c);
}


// * CAN Signal Mapping — Megasquirt Advanced Broadcast, base 0x05F0
#define CFG_CAN_BASE_ID          0x05F0u

#define CFG_CAN_ID_RPM           (CFG_CAN_BASE_ID + 0u)
#define CFG_CAN_ID_ECT           (CFG_CAN_BASE_ID + 2u)
#define CFG_CAN_ID_TPS_BATT      (CFG_CAN_BASE_ID + 3u)
#define CFG_CAN_ID_GEAR          (CFG_CAN_BASE_ID + 33u)

/* Byte offsets within the 8-byte CAN frame (big-endian int16, except GEAR=int8) */
#define CFG_CAN_RPM_BYTE         6
#define CFG_CAN_ECT_BYTE         6
#define CFG_CAN_TPS_BYTE         0
#define CFG_CAN_BATT_BYTE        2
#define CFG_CAN_GEAR_BYTE        6

/* Conversion macros — result stored in can_field_t.value (uint32_t).
 * ECT: raw = °F×10 → integer °C.  BATT: raw = V×10 → stored V×100 (1150=11.50V). */
#define CFG_CAN_RPM_CONVERT(raw)   ((uint32_t)(uint16_t)(raw))
#define CFG_CAN_ECT_CONVERT(raw)   cfg_ect_convert((int16_t)(raw))
#define CFG_CAN_TPS_CONVERT(raw)   ((uint32_t)((int32_t)(raw) / 10))
#define CFG_CAN_BATT_CONVERT(raw)  ((uint32_t)((int32_t)(raw) * 10))
#define CFG_CAN_GEAR_CONVERT(raw)  ((uint32_t)(int8_t)(raw))

/* Field freshness timeout — used by staleness checks (ms) */
#define CFG_CAN_STALE_MS         500u


// * Robustness / Watchdogs

/* Consecutive display cycles where every vv_update_field fails before
 * the comm watchdog trips (250 ms total silence). */
#define CFG_DISP_FAIL_CYCLES     5u

/* Interval between uart_transport_reset() recovery attempts (ms). */
#define CFG_DISP_REINIT_PERIOD_MS  1000u

/* Each monitored task must kick the watchdog within this window (ms). */
#define CFG_TASK_WD_TIMEOUT_MS   500u

// * Display — Victor Vision / Proculus (UnicView AD)

#define CFG_DISPLAY_PERIOD_MS    50u
#define CFG_VV_RESP_TIMEOUT_MS   200u

#define CFG_VP_RPM               0x000Au
#define CFG_VP_GEAR              0x0014u
#define CFG_VP_TPS               0x001Eu
#define CFG_VP_BATTERY           0x0028u
#define CFG_VP_ECT               0x0032u


// * LED Strip (WS2812)

#define CFG_LED_TOTAL_COUNT      5u
#define CFG_HEARTBEAT_LED_IDX    0u

/* Heartbeat status colors {R, G, B} */
#define CFG_HB_COLOR_OK          {   0u,  20u,   0u }
#define CFG_HB_COLOR_WARN        {  20u,  12u,   0u }
#define CFG_HB_COLOR_ERROR       {  20u,   0u,   0u }


// * Shifting Lights

#define SHIFT_MODE_FIXED   0   /* single RPM threshold for all gears */
#define SHIFT_MODE_GEAR    1   /* per-gear RPM thresholds            */

#define CFG_SHIFT_MODE           SHIFT_MODE_GEAR

#define CFG_SHIFT_LED_START      1u   /* first WS2812 index used for shifting */
#define CFG_SHIFT_LED_COUNT      4u   /* number of shifting LEDs              */

/* Fixed-mode thresholds — same for every gear */
#define CFG_SHIFT_RPM_START      11000u
#define CFG_SHIFT_RPM_SHIFT      13000u

/* Gear-mode thresholds — index = gear number, 0 = neutral/off */
/*                                    N      1      2      3      4      5      6   */
#define CFG_SHIFT_RPM_START_BY_GEAR  { 0u, 11000u, 11000u, 11000u, 11000u, 11000u, 11000u }
#define CFG_SHIFT_RPM_SHIFT_BY_GEAR  { 0u, 13000u, 13000u, 13000u, 13000u, 13000u, 13000u }

/* Per-LED colors {R, G, B} — index 0 = lowest RPM, index 3 = highest */
#define CFG_SHIFT_COLORS { \
    { 0x00u, 0x40u, 0x00u }, \
    { 0x28u, 0x14u, 0x00u }, \
    { 0x28u, 0x14u, 0x00u }, \
    { 0x30u, 0x00u, 0x00u } \
}

/* Flash color when rpm >= rpm_shift */
#define CFG_SHIFT_FLASH_R        0x00u
#define CFG_SHIFT_FLASH_G        0x40u
#define CFG_SHIFT_FLASH_B        0x00u

/* Flash half-period in heartbeat ticks (50 ms each → 8 ticks = 400 ms ≈ 1.3 Hz) */
#define CFG_SHIFT_FLASH_TICKS    8u

#endif /* CONFIG_H */
