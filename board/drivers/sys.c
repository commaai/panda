#include "board/sys/sys.h"

// Global critical section depth
uint8_t global_critical_depth = 0U;

// Interrupt enable state (used by EXIT_CRITICAL macro)
volatile bool interrupts_enabled = false;

// Global fault state
uint8_t fault_status = FAULT_STATUS_NONE;
uint32_t faults = 0U;
