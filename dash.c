#include "dash.h"
#include <string.h>

#define UART_TX_BUFFER_SIZE 50

static CAN_HandleTypeDef* g_hcan;
static UART_HandleTypeDef* g_huart;


__attribute__((aligned (32))) static uint8_t g_dma_buffer[UART_TX_BUFFER_SIZE];
__attribute__((aligned (32))) static uint8_t g_pending_buffer[UART_TX_BUFFER_SIZE];

static volatile uint8_t g_pending_len = 0;
static volatile uint8_t g_has_pending_data = 0;
static volatile uint8_t g_uart_tx_busy = 0;

static uint16_t g_DashValues[6] = {0};


static void DispararDMA(uint8_t len);

void DASH_Init(CAN_HandleTypeDef* hcan, UART_HandleTypeDef* huart)
{
    g_hcan = hcan;
    g_huart = huart;
    g_uart_tx_busy = 0;
    g_has_pending_data = 0;
}

void ReceberCAN(uint8_t* data, uint16_t VP, uint8_t len)
{
    uint8_t total_len = len + 6; // 3 (Header+Len) + 1 (Cmd) + 2 (VP)


    __disable_irq();

    if (g_uart_tx_busy) {

        FormataPacoteUARTDado(CMD_WRITE_VP, VP, data, g_pending_buffer, len);
        g_pending_len = total_len;
        g_has_pending_data = 1;
        __enable_irq();
    }
    else {
        g_uart_tx_busy = 1;
        // Otimização: Formata DIRETO no buffer do DMA.
        FormataPacoteUARTDado(CMD_WRITE_VP, VP, data, g_dma_buffer, len);
        __enable_irq(); // FIM DA SEÇÃO CRÍTICA

        DispararDMA(total_len);
    }
}

void DASH_SetData(uint8_t index, uint16_t value)
{
    if (index < 6) {
        g_DashValues[index] = value;
    }
}

void DASH_SendBatch(uint16_t startVP)
{
    uint8_t data_len = 12; // 6 words * 2 bytes
    uint8_t total_len = data_len + 6;
    uint8_t raw_data[12];

    for (int i = 0; i < 6; i++) {
        raw_data[i*2]     = (uint8_t)((g_DashValues[i] >> 8) & 0xFF);
        raw_data[i*2 + 1] = (uint8_t)(g_DashValues[i] & 0xFF);
    }

    __disable_irq(); // INÍCIO DA SEÇÃO CRÍTICA

    if (g_uart_tx_busy) {
        FormataPacoteUARTDado(CMD_WRITE_VP, startVP, raw_data, g_pending_buffer, data_len);
        g_pending_len = total_len;
        g_has_pending_data = 1;
        __enable_irq(); // FIM DA SEÇÃO CRÍTICA
    } else {
        g_uart_tx_busy = 1;
        FormataPacoteUARTDado(CMD_WRITE_VP, startVP, raw_data, g_dma_buffer, data_len);
        __enable_irq(); // FIM DA SEÇÃO CRÍTICA

        DispararDMA(total_len);
    }
}

static void DispararDMA(uint8_t len)
{
    if (len > UART_TX_BUFFER_SIZE) {
        g_uart_tx_busy = 0;
        return;
    }


    SCB_CleanDCache_by_Addr((uint32_t*)g_dma_buffer, ((len + 31) / 32) * 32);

    if (HAL_UART_Transmit_DMA(g_huart, g_dma_buffer, len) != HAL_OK) {
        g_uart_tx_busy = 0;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == g_huart->Instance) {
        if (g_has_pending_data) {
            g_has_pending_data = 0;


            memcpy(g_dma_buffer, g_pending_buffer, g_pending_len);

            SCB_CleanDCache_by_Addr((uint32_t*)g_dma_buffer, ((g_pending_len + 31) / 32) * 32);
            HAL_UART_Transmit_DMA(g_huart, g_dma_buffer, g_pending_len);
        } else {
            g_uart_tx_busy = 0;
        }
    }
}

void FormataPacoteUARTDado(uint8_t func, uint16_t VP, uint8_t* data, uint8_t* packetUART, uint8_t len)
{
    packetUART[0] = 0x5A; // HEADER HIGH
    packetUART[1] = 0xA5; // HEADER LOW
    packetUART[2] = 3 + len;
    packetUART[3] = func;
    packetUART[4] = (uint8_t)((VP >> 8) & 0xFF);
    packetUART[5] = (uint8_t)(VP & 0xFF);

    if (len > 0 && data != NULL) {
        memcpy(&packetUART[6], data, len);
    }
}
