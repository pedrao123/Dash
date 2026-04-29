#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>

void uart_transport_init(void);
bool uart_transport_send(const uint8_t *data, uint16_t len);
void uart_transport_set_rx_callback(void (*cb)(const uint8_t *data, uint16_t len));

#endif /* UART_TRANSPORT_H */
