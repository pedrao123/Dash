#include "dash_config.h"
#include "config.h"
#include "config_store.h"
#include "cmsis_os.h"
#include <stddef.h>

static const dash_config_t k_defaults = {
    .shift = {
        .mode              = CFG_SHIFT_MODE,
        .rpm_start         = CFG_SHIFT_RPM_START,
        .rpm_shift         = CFG_SHIFT_RPM_SHIFT,
        .rpm_start_by_gear = CFG_SHIFT_RPM_START_BY_GEAR,
        .rpm_shift_by_gear = CFG_SHIFT_RPM_SHIFT_BY_GEAR,
        .colors            = CFG_SHIFT_COLORS,
        .flash             = { CFG_SHIFT_FLASH_R, CFG_SHIFT_FLASH_G, CFG_SHIFT_FLASH_B },
        .flash_ticks       = CFG_SHIFT_FLASH_TICKS,
    },
    .hb_colors = {
        [HB_STATUS_OK]    = CFG_HB_COLOR_OK,
        [HB_STATUS_WARN]  = CFG_HB_COLOR_WARN,
        [HB_STATUS_ERROR] = CFG_HB_COLOR_ERROR,
    },
    .display = {
        .vp = {
            [VV_FIELD_RPM]     = CFG_VP_RPM,
            [VV_FIELD_GEAR]    = CFG_VP_GEAR,
            [VV_FIELD_TPS]     = CFG_VP_TPS,
            [VV_FIELD_BATTERY] = CFG_VP_BATTERY,
            [VV_FIELD_ECT]     = CFG_VP_ECT,
        },
        .resp_timeout_ms = CFG_VV_RESP_TIMEOUT_MS,
    },
    .display_period_ms     = CFG_DISPLAY_PERIOD_MS,
    .disp_fail_cycles      = CFG_DISP_FAIL_CYCLES,
    .disp_reinit_period_ms = CFG_DISP_REINIT_PERIOD_MS,
    .can_stale_ms          = CFG_CAN_STALE_MS,
};

static dash_config_t     s_active;
static volatile uint32_t s_generation;
static osMutexId_t       s_mutex;
static dash_config_src_t s_source;
static dash_config_t     s_scratch;   /* static: flash load/save copy, off the stack */

_Static_assert(sizeof(dash_config_t) <= CONFIG_STORE_MAX_LEN, "dash_config_t too big for config_store");

/* Newest flash record if it matches this firmware, otherwise defaults.
 * The size check also catches a layout change without a version bump. */
static dash_config_src_t load_from_flash(dash_config_t *out) {
    uint16_t len, version;

    config_store_init();
    if (!config_store_load(out, sizeof(*out), &len, &version)) return DASH_CFG_SRC_DEFAULTS_EMPTY;
    if (version != DASH_CONFIG_VERSION)                      return DASH_CFG_SRC_DEFAULTS_VERSION;
    if (len != sizeof(*out) || dash_config_check(out))       return DASH_CFG_SRC_DEFAULTS_INVALID;
    return DASH_CFG_SRC_FLASH;
}

void dash_config_init(void) {
    s_mutex  = osMutexNew(NULL);
    s_source = load_from_flash(&s_scratch);
    s_active = (s_source == DASH_CFG_SRC_FLASH) ? s_scratch : k_defaults;
    s_generation = 1u;
}

dash_config_src_t dash_config_source(void) {
    return s_source;
}

bool dash_config_save(void) {
    dash_config_get(&s_scratch);
    if (!config_store_save(&s_scratch, sizeof(s_scratch), DASH_CONFIG_VERSION)) return false;
    s_source = DASH_CFG_SRC_FLASH;
    return true;
}

void dash_config_defaults(dash_config_t *out) {
    *out = k_defaults;
}

uint32_t dash_config_get(dash_config_t *out) {
    osMutexAcquire(s_mutex, osWaitForever);
    *out = s_active;
    uint32_t gen = s_generation;
    osMutexRelease(s_mutex);
    return gen;
}

uint32_t dash_config_generation(void) {
    return s_generation;
}

bool dash_config_set(const dash_config_t *cfg) {
    if (dash_config_check(cfg) != NULL) return false;
    osMutexAcquire(s_mutex, osWaitForever);
    s_active = *cfg;
    s_generation++;
    osMutexRelease(s_mutex);
    return true;
}

const char *dash_config_check(const dash_config_t *cfg) {
    const shift_cfg_t *s = &cfg->shift;

    if (s->mode != SHIFT_MODE_FIXED && s->mode != SHIFT_MODE_GEAR) return "shift.mode invalido";
    if (s->flash_ticks == 0u) return "shift.flash_ticks deve ser > 0";
    if (s->rpm_shift <= s->rpm_start) return "shift.rpm_shift deve ser > shift.rpm_start";
    for (uint8_t g = 1u; g < SHIFT_GEAR_COUNT; g++)   /* gear 0 = neutral, unused */
        if (s->rpm_shift_by_gear[g] <= s->rpm_start_by_gear[g])
            return "shift.rpm_shift_by_gear deve ser > shift.rpm_start_by_gear";

    if (cfg->display.resp_timeout_ms == 0u) return "display.resp_timeout_ms deve ser > 0";
    if (cfg->display_period_ms < 10u) return "display_period_ms deve ser >= 10";
    if (cfg->disp_fail_cycles == 0u) return "disp_fail_cycles deve ser > 0";
    if (cfg->disp_reinit_period_ms == 0u) return "disp_reinit_period_ms deve ser > 0";
    if (cfg->can_stale_ms == 0u) return "can_stale_ms deve ser > 0";

    return NULL;
}
