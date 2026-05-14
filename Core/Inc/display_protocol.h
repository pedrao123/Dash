#ifndef DISPLAY_PROTOCOL_H
#define DISPLAY_PROTOCOL_H

#include <stdint.h>

// Victor Vision VP addresses — valores em decimal no UnicView AD: 10, 20, 30, 40, 50
#define VP_RPM              0x000Au
#define VP_GEAR             0x0014u
#define VP_TPS              0x001Eu
#define VP_BATTERY_VOLTAGE  0x0028u
#define VP_ECT              0x0032u

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
