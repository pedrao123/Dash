#include "uart_transport.h"
#include "cmsis_os.h"
#include <string.h>

#define RX_BUF_SIZE  64u
#define TX_BUF_SIZE  UART_TRANSPORT_TX_MAX

struct uart_transport {
    UART_HandleTypeDef *huart;
    void              (*rx_cb)(const uint8_t *, uint16_t);
    uint16_t            rx_old_pos;   // next unread index in rx_buf (ISR-only)
    osMutexId_t         tx_mutex;
    osSemaphoreId_t     tx_done;
};

static struct uart_transport s_pool[UART_TRANSPORT_MAX];
static uint8_t               s_count;

// DTCM (0x20000000) is CPU-only on STM32F722 — DMA requires AXI SRAM (SRAM1).
__attribute__((section(".sram1_bss"))) static uint8_t s_rx_buf[UART_TRANSPORT_MAX][RX_BUF_SIZE];
__attribute__((section(".sram1_bss"))) static uint8_t s_tx_buf[UART_TRANSPORT_MAX][TX_BUF_SIZE];

static inline uint8_t idx_of(const uart_transport_t *t) {
    return (uint8_t)(t - s_pool);
}

static uart_transport_t *find(const UART_HandleTypeDef *huart) {
    for (uint8_t i = 0; i < s_count; i++)
        if (s_pool[i].huart == huart) return &s_pool[i];
    return NULL;
}

#define TX_TIMEOUT_MIN_MS  5u   // original fixed value: 8 bytes @ 115200 = ~0.7ms; 5ms = 7x margin

// Long frames (config port) scale with frame time, 2x margin:
// 128 bytes @ 115200 ≈ 11 ms → 22 ms. Short display frames keep the original 5 ms.
static uint32_t tx_timeout(const uart_transport_t *t, uint16_t len) {
    uint32_t ms = 2u * (((uint32_t)len * 10000u) / t->huart->Init.BaudRate);
    if (ms < TX_TIMEOUT_MIN_MS) ms = TX_TIMEOUT_MIN_MS;
    return pdMS_TO_TICKS(ms);
}

static void rx_restart(uart_transport_t *t) {
    t->rx_old_pos = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(t->huart, s_rx_buf[idx_of(t)], RX_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(t->huart->hdmarx, DMA_IT_HT);
}

uart_transport_t *uart_transport_init(UART_HandleTypeDef *huart) {
    if (s_count >= UART_TRANSPORT_MAX || find(huart)) return NULL;

    uart_transport_t *t = &s_pool[s_count++];
    t->huart    = huart;
    t->rx_cb    = NULL;
    t->tx_mutex = osMutexNew(NULL);
    t->tx_done  = osSemaphoreNew(1, 0, NULL);
    rx_restart(t);
    return t;
}

void uart_transport_reset(uart_transport_t *t) {
    /* Abort resets gState from BUSY_TX back to READY and stops both DMAs,
     * so the next HAL_UART_Transmit_DMA call won't return HAL_BUSY.
     * The mutex is always released on send timeout, so it is free here.
     * Drain any stale TxCplt token that fires after the abort. */
    HAL_UART_Abort(t->huart);
    osSemaphoreAcquire(t->tx_done, 0);
    rx_restart(t);
}

void uart_transport_set_rx_callback(uart_transport_t *t, void (*cb)(const uint8_t *data, uint16_t len)) {
    t->rx_cb = cb;
}

bool uart_transport_send(uart_transport_t *t, const uint8_t *data, uint16_t len) {
    if (len == 0 || len > TX_BUF_SIZE) return false;

    uint32_t timeout = tx_timeout(t, len);
    if (osMutexAcquire(t->tx_mutex, timeout) != osOK) return false;

    // Copy caller's buffer (may be on stack/DTCM) into DMA-accessible SRAM1.
    uint8_t *buf = s_tx_buf[idx_of(t)];
    memcpy(buf, data, len);

    if (HAL_UART_Transmit_DMA(t->huart, buf, len) != HAL_OK) {
        osMutexRelease(t->tx_mutex);
        return false;
    }

    bool ok = (osSemaphoreAcquire(t->tx_done, timeout) == osOK);
    osMutexRelease(t->tx_mutex);
    return ok;
}

//  HAL callbacks (override weak symbols)

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    uart_transport_t *t = find(huart);
    if (t) osSemaphoreRelease(t->tx_done);  // ISR-safe in CMSIS-RTOS v2
}

/* Circular RX DMA: reception never stops and `pos` is the absolute write
 * index (RX_BUF_SIZE on TC). Deliver only [rx_old_pos, pos), split at the wrap. */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t pos) {
    uart_transport_t *t = find(huart);
    if (!t) return;

    const uint8_t *buf = s_rx_buf[idx_of(t)];
    uint16_t old = t->rx_old_pos;

    if (pos != old && t->rx_cb) {
        if (pos > old) {
            t->rx_cb(&buf[old], pos - old);
        } else {
            t->rx_cb(&buf[old], RX_BUF_SIZE - old);
            if (pos > 0) t->rx_cb(buf, pos);
        }
    }
    t->rx_old_pos = (pos == RX_BUF_SIZE) ? 0 : pos;
}
