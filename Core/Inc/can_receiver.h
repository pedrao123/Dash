#ifndef CAN_RECEIVER_H
#define CAN_RECEIVER_H

#include "can_data.h"

void can_receiver_init(void);
void can_receiver_get_state(vehicle_state_t *out);  // atomic copy with mutex
void can_receiver_task_run(void);                   // call in a loop from Task_CAN_Rx

#endif /* CAN_RECEIVER_H */
