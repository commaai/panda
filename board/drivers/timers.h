#pragma once

#include <stdint.h>
#include "stm32h7/inc/stm32h725xx.h"

void microsecond_timer_init(void);
uint32_t microsecond_timer_get(void);
void interrupt_timer_init(void);
void tick_timer_init(void);
