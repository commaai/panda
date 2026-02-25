#ifdef STM32H7
#include "stm32h7xx.h"
#else
// Stubs for native builds
// cppcheck-suppress misra-c2012-21.1 ; CMSIS-compatible stubs for non-STM32 builds
#define __enable_irq()
// cppcheck-suppress misra-c2012-21.1
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
