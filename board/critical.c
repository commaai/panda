#include "critical.h"

uint8_t global_critical_depth = 0U;
volatile bool interrupts_enabled = false;

void enable_interrupts(void) {
  interrupts_enabled = true;
#ifdef STM32H7
  __enable_irq();
#endif
}

void disable_interrupts(void) {
  interrupts_enabled = false;
#ifdef STM32H7
  __disable_irq();
#endif
}
