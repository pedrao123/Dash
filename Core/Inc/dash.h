#ifndef DASH_H
#define DASH_H

#include "main.h"
#include <stdint.h>
#include <stddef.h>

#define FRAME_HEADER_HIGH   0x5A
#define FRAME_HEADER_LOW    0xA5

#define CMD_WRITE_VP        0x82
#define CMD_READ_VP         0x83

void DASH_Init(CAN_HandleTypeDef* hcan, UART_HandleTypeDef* huart);
void ReceberCAN(uint8_t* data, uint16_t VP, uint8_t len);
void DASH_SetData(uint8_t index, uint16_t value);
void DASH_SendBatch(uint16_t startVP);
void FormataPacoteUARTDado(uint8_t func, uint16_t VP, uint8_t* data, uint8_t* packetUART, uint8_t len);

#endif /* DASH_H */
