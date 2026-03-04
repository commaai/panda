#ifndef DRIVERS_INTERRUPTS_H
#define DRIVERS_INTERRUPTS_H

#include <stdbool.h>

void unused_interrupt_handler(void);
void handle_interrupt(IRQn_Type irq_type);
void interrupt_timer_handler(void);
void init_interrupts(bool check_rate_limit);

#endif
