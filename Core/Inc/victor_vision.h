#ifndef VICTOR_VISION_H
#define VICTOR_VISION_H

#include <stdint.h>
#include "display_protocol.h"

typedef enum {
    VV_FIELD_RPM     = 0,
    VV_FIELD_GEAR,
    VV_FIELD_TPS,
    VV_FIELD_BATTERY,
    VV_FIELD_ECT,
} vv_field_t;

void          vv_init(void);
disp_result_t vv_request_field(uint8_t field_id, uint32_t *out_value);
disp_result_t vv_update_field(uint8_t field_id, uint32_t value);

#endif /* VICTOR_VISION_H */
