#include "uart_transport.h"
#include "stm32f7xx_hal.h"
#include "cmsis_os.h"

#define RX_BUF_SIZE  64u
#define TX_TIMEOUT   pdMS_TO_TICKS(100)

extern UART_HandleTypeDef  huart3;
extern DMA_HandleTypeDef   hdma_usart3_rx;

static uint8_t  s_rx_buf[RX_BUF_SIZE];
static void   (*s_rx_cb)(const uint8_t *, uint16_t);

static osMutexId_t     s_tx_mutex;
static osSemaphoreId_t s_tx_done;

static void rx_restart(void) {
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, s_rx_buf, RX_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
}

void uart_transport_init(void) {
    s_tx_mutex = osMutexNew(NULL);
    s_tx_done  = osSemaphoreNew(1, 0, NULL);
    rx_restart();
}

void uart_transport_set_rx_callback(void (*cb)(const uint8_t *data, uint16_t len)) {
    s_rx_cb = cb;
}

bool uart_transport_send(const uint8_t *data, uint16_t len) {
    if (osMutexAcquire(s_tx_mutex, TX_TIMEOUT) != osOK) return false;

    if (HAL_UART_Transmit_DMA(&huart3, (uint8_t *)data, len) != HAL_OK) {
        osMutexRelease(s_tx_mutex);
        return false;
    }

    bool ok = (osSemaphoreAcquire(s_tx_done, TX_TIMEOUT) == osOK);
    osMutexRelease(s_tx_mutex);
    return ok;
}

// --- HAL callbacks (override weak symbols) -----------------------------------

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART3) return;
    osSemaphoreRelease(s_tx_done);  // ISR-safe in CMSIS-RTOS v2
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
    if (huart->Instance != USART3) return;
    if (s_rx_cb && size > 0) s_rx_cb(s_rx_buf, size);
    rx_restart();
}
