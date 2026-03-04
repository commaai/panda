#ifndef DRIVERS_TIMERS_H
#define DRIVERS_TIMERS_H

#include <stdint.h>

void microsecond_timer_init(void);
uint32_t microsecond_timer_get(void);
void interrupt_timer_init(void);
void tick_timer_init(void);

#endif
