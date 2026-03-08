#include "board/sys/sys.h"
#include "board/can.h"

// Global critical section depth
uint8_t global_critical_depth = 0U;

// Global fault state
uint8_t fault_status = FAULT_STATUS_NONE;
uint32_t faults = 0U;
