#ifndef DRIVERS_SIMPLE_WATCHDOG_H
#define DRIVERS_SIMPLE_WATCHDOG_H

#include <stdint.h>

void simple_watchdog_kick(void);
void simple_watchdog_init(uint32_t fault, uint32_t threshold);

#endif
