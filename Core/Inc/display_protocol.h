#ifndef DISPLAY_PROTOCOL_H
#define DISPLAY_PROTOCOL_H

#include <stdint.h>

// Victor Vision VP addresses — fill in from protocol documentation
#define VP_RPM              0x0010u
#define VP_GEAR             0x0020u
#define VP_TPS              0x0030u
#define VP_BATTERY_VOLTAGE  0x0040u
#define VP_ECT              0x0050u

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
