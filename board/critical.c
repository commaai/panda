#include "critical.h"

// ********************* Critical section helpers *********************
uint8_t global_critical_depth = 0U;

void enable_interrupts(void) {
  interrupts_enabled = true;
  __enable_irq();
}

void disable_interrupts(void) {
  interrupts_enabled = false;
  __disable_irq();
}
