#ifndef DRIVERS_FAN_H
#define DRIVERS_FAN_H

#include <stdint.h>

void fan_set_power(uint8_t percentage);
void fan_init(void);
void fan_tick(void);

#endif
