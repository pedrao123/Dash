#include "task_watchdog.h"
#include "cmsis_os.h"

static volatile uint32_t s_last_kick_ms[TASK_WD_COUNT];
static volatile bool     s_comm_failed;

void task_watchdog_kick(uint8_t id) {
    if (id < TASK_WD_COUNT)
        s_last_kick_ms[id] = osKernelGetTickCount();
}

bool task_watchdog_ok(uint8_t id, uint32_t timeout_ms) {
    if (id >= TASK_WD_COUNT) return false;
    uint32_t now = osKernelGetTickCount();
    /* Grace period on startup: don't report failure before tasks have had a
     * chance to kick (2× timeout after boot). */
    if (now < timeout_ms * 2u) return true;
    return (now - s_last_kick_ms[id]) <= timeout_ms;
}

void comm_watchdog_set_failed(bool failed) {
    s_comm_failed = failed;
}

bool comm_watchdog_is_failed(void) {
    return s_comm_failed;
}
