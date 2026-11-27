#ifndef BOARD_SYS_CRITICAL_H
#define BOARD_SYS_CRITICAL_H

#include "board/sys/sys.h"

// ********************* Critical section helpers *********************
static uint8_t global_critical_depth = 0U;

static volatile bool interrupts_enabled = false;

static void enable_interrupts(void) {
  interrupts_enabled = true;
  __enable_irq();
}

static void disable_interrupts(void) {
  interrupts_enabled = false;
  __disable_irq();
}

#endif  // BOARD_SYS_CRITICAL_H
