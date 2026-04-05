#ifndef CRITICAL_H
#define CRITICAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef STM32H7
#include "stm32h7xx.h"
#endif

void enable_interrupts(void);
void disable_interrupts(void);

extern uint8_t global_critical_depth;
extern volatile bool interrupts_enabled;

#ifndef ENTER_CRITICAL
#define ENTER_CRITICAL()                                      \
  __disable_irq();                                            \
  global_critical_depth += 1U;
#endif

#ifndef EXIT_CRITICAL
#define EXIT_CRITICAL()                                       \
  global_critical_depth -= 1U;                                \
  if ((global_critical_depth == 0U) && interrupts_enabled) {  \
    __enable_irq();                                           \
  }
#endif

#endif
