#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "board/drivers/driver_declarations.h"

#ifdef STM32H7
#include "stm32h7xx.h"
#endif

extern float interrupt_load;

void unused_interrupt_handler(void);
void handle_interrupt(IRQn_Type irq_type);
void interrupt_timer_handler(void);
void init_interrupts(bool check_rate_limit);

#endif
