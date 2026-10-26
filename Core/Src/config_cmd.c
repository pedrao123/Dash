#include "config_cmd.h"
#include "dash_config.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define LINE_MAX  64u
#define OUT_MAX   96u
#define RPM_MAX   20000u

//  Parameter table — one entry per dash_config_t field

typedef enum { P_U32, P_U8, P_VP, P_RGB, P_MODE } ptype_t;

typedef struct {
    const char         *name;
    uint8_t             type;
    uint16_t            offset;   /* offsetof element [0] */
    uint8_t             first;    /* first valid index (arrays) */
    uint8_t             count;    /* 0 = scalar */
    const char *const  *labels;   /* index names instead of numbers, or NULL */
    uint32_t            min, max;
} param_t;

static const char *const k_vp_labels[VV_FIELD_COUNT] = {
    [VV_FIELD_RPM]     = "rpm",
    [VV_FIELD_GEAR]    = "gear",
    [VV_FIELD_TPS]     = "tps",
    [VV_FIELD_BATTERY] = "battery",
    [VV_FIELD_ECT]     = "ect",
};

static const char *const k_hb_labels[HB_STATUS_COUNT] = {
    [HB_STATUS_OK]    = "ok",
    [HB_STATUS_WARN]  = "warn",
    [HB_STATUS_ERROR] = "error",
};

#define OFF(f)  ((uint16_t)offsetof(dash_config_t, f))

static const param_t k_params[] = {
    { "shift.mode",              P_MODE, OFF(shift.mode),              0, 0,                     NULL,        0, 1 },
    { "shift.rpm_start",         P_U32,  OFF(shift.rpm_start),         0, 0,                     NULL,        0, RPM_MAX },
    { "shift.rpm_shift",         P_U32,  OFF(shift.rpm_shift),         0, 0,                     NULL,        0, RPM_MAX },
    { "shift.rpm_start_by_gear", P_U32,  OFF(shift.rpm_start_by_gear), 1, SHIFT_GEAR_COUNT - 1u, NULL,        0, RPM_MAX },
    { "shift.rpm_shift_by_gear", P_U32,  OFF(shift.rpm_shift_by_gear), 1, SHIFT_GEAR_COUNT - 1u, NULL,        0, RPM_MAX },
    { "shift.colors",            P_RGB,  OFF(shift.colors),            0, CFG_SHIFT_LED_COUNT,   NULL,        0, 255 },
    { "shift.flash",             P_RGB,  OFF(shift.flash),             0, 0,                     NULL,        0, 255 },
    { "shift.flash_ticks",       P_U8,   OFF(shift.flash_ticks),       0, 0,                     NULL,        1, 255 },
    { "hb_colors",               P_RGB,  OFF(hb_colors),               0, HB_STATUS_COUNT,       k_hb_labels, 0, 255 },
    { "display.vp",              P_VP,   OFF(display.vp),              0, VV_FIELD_COUNT,        k_vp_labels, 0, 0xFFFFu },
    { "display.resp_timeout_ms", P_U32,  OFF(display.resp_timeout_ms), 0, 0,                     NULL,        1, 1000 },
    { "display_period_ms",       P_U32,  OFF(display_period_ms),       0, 0,                     NULL,        10, 1000 },
    { "disp_fail_cycles",        P_U32,  OFF(disp_fail_cycles),        0, 0,                     NULL,        1, 100 },
    { "disp_reinit_period_ms",   P_U32,  OFF(disp_reinit_period_ms),   0, 0,                     NULL,        100, 60000 },
    { "can_stale_ms",            P_U32,  OFF(can_stale_ms),            0, 0,                     NULL,        50, 10000 },
};
#define PARAM_COUNT  (sizeof(k_params) / sizeof(k_params[0]))

static uint8_t type_size(uint8_t type) {
    switch (type) {
    case P_U32: return 4u;
    case P_VP:  return 2u;
    case P_RGB: return 3u;
    default:    return 1u;   /* P_U8, P_MODE */
    }
}

static void *field_ptr(dash_config_t *cfg, const param_t *p, uint8_t idx) {
    return (uint8_t *)cfg + p->offset + (size_t)idx * type_size(p->type);
}

//  State

static config_cmd_write_fn s_write;
static char                s_line[LINE_MAX + 1u];
static uint8_t             s_line_len;
static bool                s_line_overflow;
static bool                s_unsaved;
static dash_config_t       s_work;   /* static: keeps the copy off the task stack */

//  Output

static void emit(const char *fmt, ...) {
    char out[OUT_MAX];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, sizeof(out) - 2u, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n > sizeof(out) - 3u) n = (int)(sizeof(out) - 3u);   /* truncated */
    out[n++] = '\r';
    out[n++] = '\n';
    s_write(out, (uint16_t)n);
}

static void key_str(char *buf, size_t size, const param_t *p, uint8_t idx) {
    if (p->count == 0u)   snprintf(buf, size, "%s", p->name);
    else if (p->labels)   snprintf(buf, size, "%s.%s", p->name, p->labels[idx]);
    else                  snprintf(buf, size, "%s.%u", p->name, idx);
}

static void emit_param(dash_config_t *cfg, const param_t *p, uint8_t idx) {
    char key[40];
    key_str(key, sizeof(key), p, idx);
    const void *f = field_ptr(cfg, p, idx);

    switch (p->type) {
    case P_U32: { uint32_t v; memcpy(&v, f, 4); emit("%s=%lu", key, (unsigned long)v); break; }
    case P_U8:  emit("%s=%u", key, *(const uint8_t *)f); break;
    case P_VP:  { uint16_t v; memcpy(&v, f, 2); emit("%s=0x%04X", key, v); break; }
    case P_RGB: { const rgb_t *c = f; emit("%s=%u,%u,%u", key, c->r, c->g, c->b); break; }
    case P_MODE:
        emit("%s=%s", key, *(const uint8_t *)f == SHIFT_MODE_GEAR ? "gear" : "fixed");
        break;
    }
}

//  Parsing

/* Unsigned decimal or 0x hex, nothing after it. */
static bool parse_uint(const char *s, uint32_t *out) {
    if (*s < '0' || *s > '9') return false;
    char *end;
    unsigned long v = strtoul(s, &end, 0);
    if (*end != '\0') return false;
    *out = (uint32_t)v;
    return true;
}

/* "r,g,b" — spaces around the commas allowed. */
static bool parse_rgb(char *s, rgb_t *out) {
    uint32_t c[3];
    char *save;
    for (uint8_t i = 0; i < 3u; i++) {
        char *tok = strtok_r(i == 0u ? s : NULL, ",", &save);
        if (!tok) return false;
        while (*tok == ' ') tok++;
        size_t n = strlen(tok);
        while (n > 0u && tok[n - 1u] == ' ') tok[--n] = '\0';
        if (!parse_uint(tok, &c[i]) || c[i] > 255u) return false;
    }
    if (strtok_r(NULL, ",", &save)) return false;
    out->r = (uint8_t)c[0]; out->g = (uint8_t)c[1]; out->b = (uint8_t)c[2];
    return true;
}

/* Finds the param and index for a key such as "shift.colors.2" or "display.vp.rpm".
 * Returns NULL on success, otherwise the error reason. */
static const char *resolve(const char *key, const param_t **out_p, uint8_t *out_idx) {
    for (size_t i = 0; i < PARAM_COUNT; i++) {
        const param_t *p = &k_params[i];
        size_t n = strlen(p->name);
        if (strncasecmp(key, p->name, n) != 0) continue;

        const char *rest = key + n;
        if (p->count == 0u) {
            if (*rest != '\0') continue;   /* e.g. "shift.rpm_start" vs "shift.rpm_start_by_gear" */
            *out_p = p; *out_idx = 0;
            return NULL;
        }
        if (*rest != '.') continue;
        rest++;

        if (p->labels) {
            for (uint8_t k = 0; k < p->count; k++) {
                if (strcasecmp(rest, p->labels[k]) == 0) {
                    *out_p = p; *out_idx = k;
                    return NULL;
                }
            }
            return "indice invalido";
        }
        uint32_t idx;
        if (!parse_uint(rest, &idx) || idx < p->first || idx >= (uint32_t)p->first + p->count)
            return "indice invalido";
        *out_p = p; *out_idx = (uint8_t)idx;
        return NULL;
    }
    return "chave desconhecida";
}

/* Writes value into cfg. Returns NULL on success, otherwise the error reason. */
static const char *store(dash_config_t *cfg, const param_t *p, uint8_t idx, char *value) {
    void *f = field_ptr(cfg, p, idx);

    if (p->type == P_RGB) {
        rgb_t c;
        if (!parse_rgb(value, &c)) return "cor deve ser r,g,b (0-255)";
        memcpy(f, &c, sizeof(c));
        return NULL;
    }

    uint32_t v;
    if (p->type == P_MODE && strcasecmp(value, "fixed") == 0)     v = SHIFT_MODE_FIXED;
    else if (p->type == P_MODE && strcasecmp(value, "gear") == 0) v = SHIFT_MODE_GEAR;
    else if (!parse_uint(value, &v))                              return "valor invalido";

    if (v < p->min || v > p->max) return "fora da faixa";

    switch (p->type) {
    case P_U32: memcpy(f, &v, 4); break;
    case P_VP:  { uint16_t w = (uint16_t)v; memcpy(f, &w, 2); break; }
    default:    *(uint8_t *)f = (uint8_t)v; break;   /* P_U8, P_MODE */
    }
    return NULL;
}

//  Commands

static void cmd_get(char *key) {
    const param_t *p; uint8_t idx;
    const char *err = resolve(key, &p, &idx);
    if (err) { emit("ERRO %s", err); return; }
    dash_config_get(&s_work);
    emit_param(&s_work, p, idx);
    emit("OK");
}

static void cmd_set(char *key, char *value) {
    if (!value || *value == '\0') { emit("ERRO falta o valor"); return; }

    const param_t *p; uint8_t idx;
    const char *err = resolve(key, &p, &idx);
    if (err) { emit("ERRO %s", err); return; }

    dash_config_get(&s_work);
    err = store(&s_work, p, idx, value);
    if (!err) err = dash_config_check(&s_work);
    if (err) { emit("ERRO %s", err); return; }

    dash_config_set(&s_work);
    s_unsaved = true;
    emit_param(&s_work, p, idx);
    emit("OK");
}

static void cmd_dump(void) {
    dash_config_get(&s_work);
    for (size_t i = 0; i < PARAM_COUNT; i++) {
        const param_t *p = &k_params[i];
        if (p->count == 0u) {
            emit_param(&s_work, p, 0);
        } else {
            for (uint8_t k = p->first; k < p->first + p->count; k++)
                emit_param(&s_work, p, k);
        }
    }
    emit("OK");
}

static void cmd_defaults(void) {
    dash_config_defaults(&s_work);
    dash_config_set(&s_work);
    s_unsaved = true;
    emit("OK");
}

static void cmd_info(void) {
    emit("fw=%s", DASH_FW_VERSION);
    emit("cfg_version=%u", DASH_CONFIG_VERSION);
    static const char *const k_source[] = {
        [DASH_CFG_SRC_FLASH]            = "flash",
        [DASH_CFG_SRC_DEFAULTS_EMPTY]   = "defaults (flash vazia)",
        [DASH_CFG_SRC_DEFAULTS_VERSION] = "defaults (versao diferente)",
        [DASH_CFG_SRC_DEFAULTS_INVALID] = "defaults (registro invalido)",
    };
    emit("cfg_source=%s", k_source[dash_config_source()]);
    emit("unsaved=%u", s_unsaved ? 1u : 0u);
    emit("OK");
}

static void cmd_save(void) {
    if (!dash_config_save()) { emit("ERRO falha ao gravar na flash"); return; }
    s_unsaved = false;
    emit("OK");
}

static void execute(char *line) {
    char *save;
    char *cmd   = strtok_r(line, " \t", &save);
    if (!cmd) return;   /* blank line (e.g. the \n of a \r\n) */
    char *key   = strtok_r(NULL, " \t", &save);
    char *value = strtok_r(NULL, "", &save);   /* rest of the line */
    if (value) while (*value == ' ' || *value == '\t') value++;

    if      (strcasecmp(cmd, "GET") == 0)      { if (key) cmd_get(key);       else emit("ERRO falta a chave"); }
    else if (strcasecmp(cmd, "SET") == 0)      { if (key) cmd_set(key, value); else emit("ERRO falta a chave"); }
    else if (strcasecmp(cmd, "DUMP") == 0)     cmd_dump();
    else if (strcasecmp(cmd, "DEFAULTS") == 0) cmd_defaults();
    else if (strcasecmp(cmd, "INFO") == 0)     cmd_info();
    else if (strcasecmp(cmd, "SAVE") == 0)     cmd_save();
    else                                       emit("ERRO comando desconhecido");
}

//  Public API

void config_cmd_init(config_cmd_write_fn write) {
    s_write         = write;
    s_line_len      = 0;
    s_line_overflow = false;
    s_unsaved       = false;
}

void config_cmd_input(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c == '\r' || c == '\n') {
            if (s_line_overflow) {
                emit("ERRO linha muito longa");
            } else {
                s_line[s_line_len] = '\0';
                execute(s_line);
            }
            s_line_len      = 0;
            s_line_overflow = false;
        } else if (s_line_len < LINE_MAX) {
            s_line[s_line_len++] = c;
        } else {
            s_line_overflow = true;   /* drop the rest, report at end of line */
        }
    }
}
