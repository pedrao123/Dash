#ifndef TASK_WATCHDOG_H
#define TASK_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

#define TASK_WD_CANRX    0u
#define TASK_WD_DISPLAY  1u
#define TASK_WD_COUNT    2u

/* Task liveness watchdog */
void task_watchdog_kick(uint8_t id);
bool task_watchdog_ok(uint8_t id, uint32_t timeout_ms);

/* Display communication watchdog */
void comm_watchdog_set_failed(bool failed);
bool comm_watchdog_is_failed(void);

#endif /* TASK_WATCHDOG_H */
