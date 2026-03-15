#pragma once

#include "board/drivers/drivers.h"

extern struct fan_state_t fan_state;

void fan_set_power(uint8_t percentage);
void fan_init(void);
// Call this at FAN_TICK_FREQ
void fan_tick(void);
