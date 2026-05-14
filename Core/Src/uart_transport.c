#include "uart_transport.h"
#include "stm32f7xx_hal.h"
#include "cmsis_os.h"
#include <string.h>

#define RX_BUF_SIZE  64u
#define TX_BUF_SIZE  32u   // max frame size (write + CRC = 10 bytes)
#define TX_TIMEOUT   pdMS_TO_TICKS(100)

extern UART_HandleTypeDef  huart3;
extern DMA_HandleTypeDef   hdma_usart3_rx;

// DTCM (0x20000000) is CPU-only on STM32F722 — DMA requires AXI SRAM (SRAM1).
__attribute__((section(".sram1_bss"))) static uint8_t s_rx_buf[RX_BUF_SIZE];
__attribute__((section(".sram1_bss"))) static uint8_t s_tx_buf[TX_BUF_SIZE];

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
    if (len > TX_BUF_SIZE) return false;
    if (osMutexAcquire(s_tx_mutex, TX_TIMEOUT) != osOK) return false;

    // Copy caller's buffer (may be on stack/DTCM) into DMA-accessible SRAM1.
    memcpy(s_tx_buf, data, len);

    if (HAL_UART_Transmit_DMA(&huart3, s_tx_buf, len) != HAL_OK) {
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
