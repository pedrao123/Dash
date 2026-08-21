#include "victor_vision.h"
#include "config.h"
#include "uart_transport.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdbool.h>

//  Protocol constants

#define FRAME_HDR_H     0x5Au
#define FRAME_HDR_L     0xA5u
#define CMD_WRITE_VP    0x82u
#define CMD_READ_VP     0x83u

// BC = bytes from CMD to last DATA byte (no CRC).
#define BC_WRITE_1W     5u   // CMD(1) + VP(2) + DATA(2)
#define BC_READ_REQ_1W  4u   // CMD(1) + VP(2) + LEN(1)
#define BC_READ_RSP_1W  6u   // CMD(1) + VP(2) + LEN(1) + DATA(2)

#define RESP_TIMEOUT    pdMS_TO_TICKS(CFG_VV_RESP_TIMEOUT_MS)
#define MAX_PAYLOAD     32u

static const uint16_t s_vp_table[] = {
    [VV_FIELD_RPM]     = CFG_VP_RPM,
    [VV_FIELD_GEAR]    = CFG_VP_GEAR,
    [VV_FIELD_TPS]     = CFG_VP_TPS,
    [VV_FIELD_BATTERY] = CFG_VP_BATTERY,
    [VV_FIELD_ECT]     = CFG_VP_ECT,
};
#define FIELD_COUNT ((uint8_t)(sizeof(s_vp_table) / sizeof(s_vp_table[0])))

//  Frame builders

static uint16_t frame_write_vp(uint8_t *buf, uint16_t vp, uint16_t val) {
    buf[0] = FRAME_HDR_H;
    buf[1] = FRAME_HDR_L;
    buf[2] = BC_WRITE_1W;
    buf[3] = CMD_WRITE_VP;
    buf[4] = (uint8_t)(vp >> 8);  buf[5] = (uint8_t)vp;
    buf[6] = (uint8_t)(val >> 8); buf[7] = (uint8_t)val;
    return 8u;
}

static uint16_t frame_read_vp(uint8_t *buf, uint16_t vp) {
    buf[0] = FRAME_HDR_H;
    buf[1] = FRAME_HDR_L;
    buf[2] = BC_READ_REQ_1W;
    buf[3] = CMD_READ_VP;
    buf[4] = (uint8_t)(vp >> 8); buf[5] = (uint8_t)vp;
    buf[6] = 1u;
    return 7u;
}

//  RX parser
// Runs in ISR context (called from uart_transport HAL callback).
// Only ISR-safe operations and osSemaphoreRelease allowed here.

typedef enum { PS_FHH, PS_FHL, PS_BC, PS_PAYLOAD } ps_t;

static struct {
    ps_t    state;
    uint8_t bc;
    uint8_t buf[MAX_PAYLOAD];  // [CMD, data...] — bc bytes total
    uint8_t cnt;
} s_rx;

static osSemaphoreId_t   s_resp_sem;
static osMutexId_t       s_req_mutex;
static uint16_t          s_pending_vp;
static volatile uint16_t s_resp_val;
static volatile bool     s_resp_ok;

static void parser_reset(void) {
    s_rx.state = PS_FHH;
    s_rx.cnt   = 0;
}

static void rx_cb(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        switch (s_rx.state) {
        case PS_FHH:
            if (b == FRAME_HDR_H) s_rx.state = PS_FHL;
            break;
        case PS_FHL:
            s_rx.state = (b == FRAME_HDR_L) ? PS_BC : PS_FHH;
            break;
        case PS_BC:
            s_rx.bc    = b;
            s_rx.cnt   = 0;
            s_rx.state = PS_PAYLOAD;
            break;
        case PS_PAYLOAD:
            if (s_rx.cnt < MAX_PAYLOAD) s_rx.buf[s_rx.cnt] = b;
            if (++s_rx.cnt == s_rx.bc) {
                // buf = [CMD, VP_H, VP_L, LEN, DATA_H, DATA_L]
                if (s_rx.buf[0] == CMD_READ_VP && s_rx.bc == BC_READ_RSP_1W) {
                    uint16_t vp  = ((uint16_t)s_rx.buf[1] << 8) | s_rx.buf[2];
                    uint16_t val = ((uint16_t)s_rx.buf[4] << 8) | s_rx.buf[5];
                    s_resp_val = val;
                    s_resp_ok  = (vp == s_pending_vp);
                    osSemaphoreRelease(s_resp_sem);
                }
                parser_reset();
            }
            break;
        }
    }
}

// Public API 

void vv_init(void) {
    s_resp_sem  = osSemaphoreNew(1, 0, NULL);
    s_req_mutex = osMutexNew(NULL);
    parser_reset();
    uart_transport_set_rx_callback(rx_cb);
}

disp_result_t vv_update_field(uint8_t field_id, uint32_t value) {
    if (field_id >= FIELD_COUNT) return DISP_TIMEOUT;
    uint8_t buf[8];
    uint16_t len = frame_write_vp(buf, s_vp_table[field_id], (uint16_t)value);
    return uart_transport_send(buf, len) ? DISP_OK : DISP_TIMEOUT;
}

disp_result_t vv_request_field(uint8_t field_id, uint32_t *out_value) {
    if (field_id >= FIELD_COUNT || !out_value) return DISP_TIMEOUT;

    osMutexAcquire(s_req_mutex, osWaitForever);

    s_pending_vp = s_vp_table[field_id];
    s_resp_ok    = false;
    osSemaphoreAcquire(s_resp_sem, 0);  // drain any stale token from a previous response

    uint8_t buf[7];
    uint16_t len = frame_read_vp(buf, s_pending_vp);

    disp_result_t result = DISP_TIMEOUT;
    if (uart_transport_send(buf, len)) {
        if (osSemaphoreAcquire(s_resp_sem, RESP_TIMEOUT) == osOK && s_resp_ok) {
            *out_value = s_resp_val;
            result = DISP_OK;
        }
    }

    osMutexRelease(s_req_mutex);
    return result;
}
