#ifndef CAN_DATA_H
#define CAN_DATA_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t value;
    uint32_t timestamp_ms;
    bool     fresh;
} can_field_t;

typedef struct {
    can_field_t rpm;
    can_field_t gear;
    can_field_t tps;             // throttle position, 0–100 %
    can_field_t battery_voltage; // escala: 1150 = 11.50 V
    can_field_t ect;             // engine coolant temperature °C
} vehicle_state_t;

#endif /* CAN_DATA_H */
