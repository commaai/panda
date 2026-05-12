#pragma once

#include <stdint.h>

void simple_watchdog_kick(void);
void simple_watchdog_init(uint32_t fault, uint32_t threshold);
