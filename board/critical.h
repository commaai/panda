#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "config.h"

// ********************* Critical section helpers *********************
extern uint8_t global_critical_depth;

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

extern uint8_t global_critical_depth;
static volatile bool interrupts_enabled;

void enable_interrupts(void);
void disable_interrupts(void);
