#ifndef DASH_CONFIG_H
#define DASH_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "shifting_lights.h"
#include "heartbeat_led.h"
#include "victor_vision.h"

#define DASH_FW_VERSION      "0.3.0"   /* bump on every release */
#define DASH_CONFIG_VERSION  1u        /* layout of dash_config_t; bump when fields change */

/* Runtime configuration — everything that can change without a reboot.
 * Defaults come from config.h. Values are published as a whole: readers
 * always get a consistent copy, never a half-updated struct. */
typedef struct {
    shift_cfg_t shift;
    rgb_t       hb_colors[HB_STATUS_COUNT];
    vv_cfg_t    display;
    uint32_t    display_period_ms;
    uint32_t    disp_fail_cycles;       /* failed cycles before comm watchdog trips */
    uint32_t    disp_reinit_period_ms;  /* interval between transport resets */
    uint32_t    can_stale_ms;           /* RPM older than this → heartbeat WARN */
} dash_config_t;

/* Where the config active since boot came from (see dash_config_init). */
typedef enum {
    DASH_CFG_SRC_FLASH = 0,          /* valid record for this DASH_CONFIG_VERSION   */
    DASH_CFG_SRC_DEFAULTS_EMPTY,     /* no valid record in flash                    */
    DASH_CFG_SRC_DEFAULTS_VERSION,   /* record from another DASH_CONFIG_VERSION     */
    DASH_CFG_SRC_DEFAULTS_INVALID,   /* right version, but wrong size or check fails */
} dash_config_src_t;

/* Call once after osKernelInitialize(), before the tasks run. Loads the
 * newest flash record, or the config.h defaults if it can't be used. */
void dash_config_init(void);

dash_config_src_t dash_config_source(void);

/* Persists the active config to flash. Usually < 1 ms; when a flash sector
 * fills (~every 800 saves) it erases one first and the CPU stalls ~1 s.
 * Call from a single task (config_store is not thread-safe). */
bool dash_config_save(void);

void dash_config_defaults(dash_config_t *out);

/* Atomic copy of the active config; returns its generation number. */
uint32_t dash_config_get(dash_config_t *out);

/* Generation of the active config — changes every time a new one is published.
 * Tasks compare it with the generation they last applied. */
uint32_t dash_config_generation(void);

/* Validates and publishes cfg. Returns false (nothing changes) if invalid. */
bool dash_config_set(const dash_config_t *cfg);

/* NULL if cfg is valid, otherwise a short reason (ASCII, for the serial port). */
const char *dash_config_check(const dash_config_t *cfg);

#endif /* DASH_CONFIG_H */
