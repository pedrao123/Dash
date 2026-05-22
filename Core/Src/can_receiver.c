#include "can_receiver.h"
#include "config.h"
#include "main.h"
#include "cmsis_os.h"
#include <string.h>

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
        /* Disable FMPIE immediately to prevent interrupt storm.
         * bxCAN FMPIE is level-triggered: the NVIC re-fires the ISR on every
         * return as long as FIFO0 is non-empty. Disabling the notification here
         * stops the re-firing; can_receiver_task_run() re-enables it after
         * draining the FIFO. */
        HAL_CAN_DeactivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
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
    f.FilterIdHigh         = (uint16_t)(CFG_CAN_ID_RPM      << 5);
    f.FilterIdLow          = (uint16_t)(CFG_CAN_ID_ECT      << 5);
    f.FilterMaskIdHigh     = (uint16_t)(CFG_CAN_ID_TPS_BATT << 5);
    f.FilterMaskIdLow      = (uint16_t)(CFG_CAN_ID_GEAR     << 5);
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
    /* Bounded wait so Task_CAN_Rx can kick the task watchdog even when
     * the bus is silent. Half the watchdog window is conservative enough. */
    osSemaphoreAcquire(s_rx_sem, pdMS_TO_TICKS(CFG_TASK_WD_TIMEOUT_MS / 2u));

    CAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    while (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &hdr, data) != HAL_OK) break;

        uint32_t id = (hdr.IDE == CAN_ID_STD) ? hdr.StdId : hdr.ExtId;

        osMutexAcquire(s_state_mutex, osWaitForever);
        switch (id) {
        case CFG_CAN_ID_RPM:
            update_field(&s_state.rpm,             CFG_CAN_RPM_CONVERT(be16(data, CFG_CAN_RPM_BYTE)));
            break;
        case CFG_CAN_ID_ECT:
            update_field(&s_state.ect,             CFG_CAN_ECT_CONVERT(be16(data, CFG_CAN_ECT_BYTE)));
            break;
        case CFG_CAN_ID_TPS_BATT:
            update_field(&s_state.tps,             CFG_CAN_TPS_CONVERT(be16(data, CFG_CAN_TPS_BYTE)));
            update_field(&s_state.battery_voltage, CFG_CAN_BATT_CONVERT(be16(data, CFG_CAN_BATT_BYTE)));
            break;
        case CFG_CAN_ID_GEAR:
            update_field(&s_state.gear,            CFG_CAN_GEAR_CONVERT(data[CFG_CAN_GEAR_BYTE]));
            break;
        default:
            break;
        }
        osMutexRelease(s_state_mutex);
    }

    /* Re-enable after FIFO is fully drained — safe now since FIFO is empty,
     * so the level-triggered IRQ won't immediately re-fire. */
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}
