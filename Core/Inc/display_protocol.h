#ifndef DISPLAY_PROTOCOL_H
#define DISPLAY_PROTOCOL_H

#include <stdint.h>

typedef struct {
    uint16_t vp;      // Victor Vision variable pointer address
    uint32_t value;
} display_update_t;

typedef enum {
    DISP_OK,
    DISP_TIMEOUT,
    DISP_CRC_ERR,
} disp_result_t;

#endif /* DISPLAY_PROTOCOL_H */
