#ifdef STM32H7
#include "stm32h7xx.h"
#else
// Stubs for native builds
#define __enable_irq()
#define __disable_irq()
#endif

#include "sys.h"

uint8_t global_critical_depth = 0U;
volatile bool interrupts_enabled = false;

void enable_interrupts(void) {
  interrupts_enabled = true;
  __enable_irq();
}

void disable_interrupts(void) {
  interrupts_enabled = false;
  __disable_irq();
}
