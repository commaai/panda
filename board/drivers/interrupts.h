#pragma once

#include "board/drivers/drivers.h"

void unused_interrupt_handler(void);

extern interrupt interrupts[NUM_INTERRUPTS];

extern float interrupt_load;

void handle_interrupt(IRQn_Type irq_type);
// Every second
void interrupt_timer_handler(void);
void init_interrupts(bool check_rate_limit);
