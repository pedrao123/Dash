#ifndef VICTOR_VISION_H
#define VICTOR_VISION_H

#include <stdint.h>
#include "display_protocol.h"
#include "uart_transport.h"

typedef enum {
    VV_FIELD_RPM     = 0,
    VV_FIELD_GEAR,
    VV_FIELD_TPS,
    VV_FIELD_BATTERY,
    VV_FIELD_ECT,
    VV_FIELD_COUNT
} vv_field_t;

typedef struct {
    uint16_t vp[VV_FIELD_COUNT];   /* VP address of each field */
    uint32_t resp_timeout_ms;      /* read (0x83) response timeout */
} vv_cfg_t;

void          vv_init(uart_transport_t *port);
void          vv_set_config(const vv_cfg_t *cfg);  /* call from the task that owns the display */
disp_result_t vv_request_field(uint8_t field_id, uint32_t *out_value);
disp_result_t vv_update_field(uint8_t field_id, uint32_t value);

#endif /* VICTOR_VISION_H */
