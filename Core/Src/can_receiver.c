#include "can_receiver.h"
#include "main.h"
#include "cmsis_os.h"
#include <string.h>

// Signal map — source: Megasquirt CAN Broadcast spec (2016-02-17)
// Advanced Real-Time Data Broadcast, base ID 1520 (0x5F0), 11-bit, big-endian, 500 kbps.
// CAN ID = base (0x5F0) + group number.
//
// ID 0x5F0 (Group  0): rpm  @ bytes 6-7, uint16, ÷1        → RPM
// ID 0x5F2 (Group  2): clt  @ bytes 6-7, int16,  ÷10 °F   → convert to integer °C
// ID 0x5F3 (Group  3): tps  @ bytes 0-1, int16,  ÷10 %    → integer %
//                      batt @ bytes 2-3, int16,  ÷10 V     → store ×100 (1150 = 11.50 V)
// ID 0x611 (Group 33): gear @ byte  6,   int8,   ÷1
//
// TunerStudio: CAN-Bus/Testmodes → CAN Realtime Data Broadcasting
//   Set base ID = 1520, enable groups 0, 2, 3, 33.

#define CAN_ID_GRP0   0x5F0u  // seconds, pw1, pw2, rpm
#define CAN_ID_GRP2   0x5F2u  // baro, map, mat, clt
#define CAN_ID_GRP3   0x5F3u  // tps, batt, afr1, afr2
#define CAN_ID_GRP33  0x611u  // duty_pwm×6, gear, status8

static inline int16_t be16(const uint8_t *d, uint8_t off) {
    return (int16_t)(((uint16_t)d[off] << 8) | d[off + 1]);
}

extern CAN_HandleTypeDef hcan1;

static osMutexId_t      s_state_mutex;
static osSemaphoreId_t  s_rx_sem;
static vehicle_state_t  s_state;

static void update_field(can_field_t *f, uint32_t value) {
    f->value        = value;
    f->timestamp_ms = osKernelGetTickCount();
    f->fresh        = true;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance == CAN1) {
        osSemaphoreRelease(s_rx_sem);
    }
}

void can_receiver_init(void) {
    s_state_mutex = osMutexNew(NULL);
    s_rx_sem      = osSemaphoreNew(1, 0, NULL);

    // 16-bit list mode: one bank holds exactly 4 standard IDs.
    // Format: STID[10:0] in bits [15:5], IDE=0, RTR=0 → value = StdId << 5.
    CAN_FilterTypeDef f = {0};
    f.FilterBank           = 0;
    f.FilterMode           = CAN_FILTERMODE_IDLIST;
    f.FilterScale          = CAN_FILTERSCALE_16BIT;
    f.FilterIdHigh         = (uint16_t)(CAN_ID_GRP0  << 5);  // 0x5F0 — rpm
    f.FilterIdLow          = (uint16_t)(CAN_ID_GRP2  << 5);  // 0x5F2 — clt
    f.FilterMaskIdHigh     = (uint16_t)(CAN_ID_GRP3  << 5);  // 0x5F3 — tps, batt
    f.FilterMaskIdLow      = (uint16_t)(CAN_ID_GRP33 << 5);  // 0x611 — gear
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation     = ENABLE;
    HAL_CAN_ConfigFilter(&hcan1, &f);

    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_Start(&hcan1);
}

void can_receiver_get_state(vehicle_state_t *out) {
    osMutexAcquire(s_state_mutex, osWaitForever);
    memcpy(out, &s_state, sizeof(s_state));
    osMutexRelease(s_state_mutex);
}

void can_receiver_task_run(void) {
    osSemaphoreAcquire(s_rx_sem, osWaitForever);

    CAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    while (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &hdr, data) != HAL_OK) break;

        uint32_t id = (hdr.IDE == CAN_ID_STD) ? hdr.StdId : hdr.ExtId;

        osMutexAcquire(s_state_mutex, osWaitForever);
        switch (id) {
        case CAN_ID_GRP0:
            update_field(&s_state.rpm, (uint32_t)(uint16_t)be16(data, 6));
            break;
        case CAN_ID_GRP2: {
            // raw = actual_°F × 10; convert to integer °C: (raw - 320) × 5 / 90
            int32_t raw_f = be16(data, 6);
            update_field(&s_state.ect, (uint32_t)((raw_f - 320) * 5 / 90));
            break;
        }
        case CAN_ID_GRP3:
            update_field(&s_state.tps,            (uint32_t)(be16(data, 0) / 10));
            // raw = actual_V × 10; store as actual_V × 100 (1150 = 11.50 V)
            update_field(&s_state.battery_voltage, (uint32_t)(be16(data, 2) * 10));
            break;
        case CAN_ID_GRP33:
            update_field(&s_state.gear, (uint32_t)(int8_t)data[6]);
            break;
        default:
            break;
        }
        osMutexRelease(s_state_mutex);
    }
}
