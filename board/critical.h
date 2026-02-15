#pragma once

#include <stdint.h>
#include <stdbool.h>

// ********************* Critical section helpers *********************
void enable_interrupts(void);
void disable_interrupts(void);

extern uint8_t global_critical_depth;
extern volatile bool interrupts_enabled;

#ifdef STM32H7
#define ENTER_CRITICAL()                                      \
  __disable_irq();                                            \
  global_critical_depth += 1U;

#define EXIT_CRITICAL()                                       \
  global_critical_depth -= 1U;                                \
  if ((global_critical_depth == 0U) && interrupts_enabled) {  \
    __enable_irq();                                           \
  }
#else
#define ENTER_CRITICAL() global_critical_depth += 1U;
#define EXIT_CRITICAL() global_critical_depth -= 1U;
#endif
