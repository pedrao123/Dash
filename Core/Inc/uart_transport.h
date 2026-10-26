#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f7xx_hal.h"

/* One instance per UART. Instances come from a static pool
 * (UART_TRANSPORT_MAX) whose DMA buffers live in SRAM1. */
typedef struct uart_transport uart_transport_t;

#define UART_TRANSPORT_MAX   2u
#define UART_TRANSPORT_TX_MAX 128u   /* largest frame accepted by send() */

/* Call after osKernelInitialize() and before osKernelStart() (the pool is not
 * thread-safe). huart must already be initialised by CubeMX with RX DMA in
 * circular mode. Returns NULL if the pool is full. */
uart_transport_t *uart_transport_init(UART_HandleTypeDef *huart);

void uart_transport_reset(uart_transport_t *t);   // abort stuck DMA and restart; call on detected TX error
bool uart_transport_send(uart_transport_t *t, const uint8_t *data, uint16_t len);

/* cb runs in ISR context and only receives bytes that are new since the last call. */
void uart_transport_set_rx_callback(uart_transport_t *t, void (*cb)(const uint8_t *data, uint16_t len));

#endif /* UART_TRANSPORT_H */
