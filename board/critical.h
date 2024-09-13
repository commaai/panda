#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "config.h"

// ********************* Critical section helpers *********************
extern volatile bool interrupts_enabled;

void enable_interrupts(void);
void disable_interrupts(void);

extern uint8_t global_critical_depth;
#define ENTER_CRITICAL()                                      \
  __disable_irq();                                            \
  global_critical_depth += 1U;

#define EXIT_CRITICAL()                                       \
  global_critical_depth -= 1U;                                \
  if ((global_critical_depth == 0U) && interrupts_enabled) {  \
    __enable_irq();                                           \
  }
