#pragma once

#include "board/sys/sys.h"

// ********************* Critical section helpers *********************
extern uint8_t global_critical_depth;
extern volatile bool interrupts_enabled;

void enable_interrupts(void);
void disable_interrupts(void);
