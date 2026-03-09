#include "board/sys/sys.h"

// ********************* Critical section helpers *********************
// global_critical_depth is declared extern in sys.h

static inline void enable_interrupts(void) {
  interrupts_enabled = true;
  __enable_irq();
}

static inline void disable_interrupts(void) {
  interrupts_enabled = false;
  __disable_irq();
}
