#ifndef DRIVERS_INTERRUPTS_H
#define DRIVERS_INTERRUPTS_H

#include "board/drivers/drivers.h"

// interrupt struct and related macros are defined in drivers.h
// This file only contains declarations

void unused_interrupt_handler(void);
void handle_interrupt(IRQn_Type irq_type);
void interrupt_timer_handler(void);
void init_interrupts(bool check_rate_limit);
void interrupt_timer_init(void);
uint32_t microsecond_timer_get(void);

#endif
