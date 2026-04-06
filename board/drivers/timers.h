// Timer driver
#pragma once

#include "../stm32h7/inc/stm32h7xx.h"

// Function declarations
static void timer_init(TIM_TypeDef *TIM, int psc);
void microsecond_timer_init(void);
uint32_t microsecond_timer_get(void);
void interrupt_timer_init(void);
void tick_timer_init(void);
