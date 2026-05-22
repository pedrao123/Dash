#ifndef SHIFTING_LIGHTS_H
#define SHIFTING_LIGHTS_H

#include <stdint.h>
#include "ws2812.h"

void shifting_lights_init(ws2812_handleTypeDef *ws2812);

/* Call every heartbeat tick (~20 ms). gear = current gear number (0 = neutral). */
void shifting_lights_update(uint32_t rpm, uint32_t gear);

#endif /* SHIFTING_LIGHTS_H */
