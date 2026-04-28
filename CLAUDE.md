# Projeto: CAN → STM32 → Display UART

## Visão Geral

Pipeline de dados de um barramento CAN para um display via UART, rodando em STM32F722 com FreeRTOS.

```
[CAN Bus] ──► STM32F722 ──► [Display Victor Vision]
               (FreeRTOS)         (UART)
```

---

## Hardware

| Item | Detalhe |
|---|---|
| MCU | STM32F722 |
| CAN | bxCAN — alta frequência de mensagens |
| Display | Victor Vision — UART, protocolo request/response |
| UART | DMA + Idle Line IRQ |
| RTOS | FreeRTOS via STM32CubeMX (CMSIS-RTOS v2) |

---

## Arquitetura de Software

### Camadas (de baixo pra cima)

```
┌─────────────────────────────────┐
│       Application Layer         │  Tasks FreeRTOS — conecta tudo
├─────────────────────────────────┤
│       Protocol Layer            │  victor_vision.h — frames Victor Vision
├─────────────────────────────────┤
│       CAN Layer                 │  can_receiver.h — snapshot com mutex
├─────────────────────────────────┤
│       Transport Layer           │  uart_transport.h — DMA + Idle Line IRQ
├─────────────────────────────────┤
│       Driver Layer              │  HAL / CubeMX gerado
└─────────────────────────────────┘
```

**Regra:** cada camada só conhece a imediatamente abaixo. A application layer é a única que conhece todas.

---

## FreeRTOS — Tasks

```
TaskCANRx     — Prioridade ALTA
  - Aguarda semáforo da ISR bxCAN
  - Filtra IDs relevantes
  - Atualiza vehicle_state_t com mutex

TaskDisplay   — Prioridade MÉDIA
  - Período fixo via vTaskDelayUntil (ex: 50ms)
  - Lê snapshot (vehicle_state_t) com mutex
  - Envia campos atualizados via Protocol Layer
  - Gerencia request/response com timeout

TaskHeartbeat — Prioridade BAIXA
  - Pisca LED
  - Monitora saúde das outras tasks
```

---

## Contratos de Dados (definidos antes de qualquer implementação)

```c
// can_data.h
typedef struct {
    uint32_t value;
    uint32_t timestamp_ms;
    bool fresh;
} can_field_t;

typedef struct {
    can_field_t rpm;
    can_field_t temp_agua;
    can_field_t velocidade;
    // adicionar campos conforme necessário
} vehicle_state_t;

// display_protocol.h
typedef struct {
    uint8_t  field_id;
    uint32_t value;
} display_update_t;

typedef enum {
    DISP_OK,
    DISP_TIMEOUT,
    DISP_CRC_ERR,
} disp_result_t;
```

---

## Transport Layer — Interface Pública

```c
// uart_transport.h
void uart_transport_init(void);
bool uart_transport_send(const uint8_t *data, uint16_t len);
void uart_transport_set_rx_callback(void (*cb)(const uint8_t *data, uint16_t len));
```

### Mecanismo interno
- TX: DMA → ISR `HAL_UART_TxCpltCallback` → `xSemaphoreGiveFromISR`
- RX: DMA circular + Idle Line IRQ → `xSemaphoreGiveFromISR`
- A camada acima recebe dados via callback — nunca vê bytes crus

---

## Protocol Layer — Interface Pública

```c
// victor_vision.h
void          vv_init(void);
disp_result_t vv_request_field(uint8_t field_id, uint32_t *out_value);
disp_result_t vv_update_field(uint8_t field_id, uint32_t value);
```

- Monta e parseia frames conforme documentação Victor Vision
- Testável com mock do transport antes do hardware real

---

## CAN Layer — Interface Pública

```c
// can_receiver.h
void can_receiver_init(void);
void can_receiver_get_state(vehicle_state_t *out);  // cópia atômica com mutex
```

- ISR bxCAN alimenta snapshot internamente
- Snapshot pattern: desacopla taxa CAN (alta) da taxa display (50–100ms)

---

## Sincronização — Regras

| Situação | Mecanismo |
|---|---|
| DMA TX terminou | Binary Semaphore (dado em ISR) |
| UART Idle Line RX | Binary Semaphore (dado em ISR) |
| CAN ISR recebeu frame | Binary Semaphore → acorda TaskCANRx |
| Timeout de resposta display | `xSemaphoreTake(sem, pdMS_TO_TICKS(N))` |
| Acesso ao vehicle_state_t | Mutex |

**Nunca usar `volatile bool` como flag entre ISR e task — sempre semáforo FreeRTOS.**

---

## Ordem de Implementação

```
Fase 1 — Base RTOS
  ├── CubeMX: FreeRTOS + UART (DMA) + bxCAN configurados
  ├── Tasks esqueleto com LED blink
  └── Validar que o scheduler sobe e tasks rodam

Fase 2 — Transport Layer (uart_transport.c)
  ├── TX com DMA + semáforo
  ├── RX com DMA circular + Idle Line IRQ + callback
  └── Teste: loopback UART (TX conectado ao RX)

Fase 3 — Protocol Layer (victor_vision.c)
  ├── Implementar framing conforme documentação Victor Vision
  ├── Parser por máquina de estados no RX
  ├── CRC e timeout de resposta
  └── Teste: mock do transport, validar frames com dados fixos

Fase 4 — CAN Layer (can_receiver.c)
  ├── ISR bxCAN → semáforo → TaskCANRx
  ├── Filtro por ID e atualização do snapshot
  └── Teste: injetar mensagens CAN e validar snapshot via UART debug

Fase 5 — Application Layer (tasks.c)
  ├── TaskDisplay: snapshot → vv_update_field
  ├── Decimação por tempo (vTaskDelayUntil 50ms)
  └── Teste de integração: CAN real → display atualizado

Fase 6 — Robustez
  ├── Watchdog de comunicação (display não responde → estado de erro)
  ├── Task watchdog no HeartBeat
  └── Teste de stress: CAN em alta frequência, UART com ruído simulado
```

---

## Referências

- Repositório de referência UART/DMA: https://github.com/MaJerle/stm32-usart-uart-dma-rx-tx
- Toolchain: STM32CubeMX + STM32CubeIDE (ou CMake + arm-none-eabi-gcc)
- FreeRTOS docs: https://www.freertos.org/RTOS.html

---

## Notas para o Claude Code

- Sempre respeitar as interfaces públicas definidas acima — não expor detalhes de DMA ou semáforos para camadas superiores
- Cada módulo deve ter seu próprio `.h` e `.c` — sem lógica de protocolo dentro do transport
- Ao gerar código, usar CMSIS-RTOS v2 API (`osMutexAcquire`, `osSemaphoreRelease`, etc.) ou FreeRTOS nativo — manter consistente em todo o projeto
- Testes de cada fase devem ser validáveis sem depender da fase seguinte estar pronta
