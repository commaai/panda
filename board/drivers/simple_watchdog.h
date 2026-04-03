#ifndef SIMPLE_WATCHDOG_H
#define SIMPLE_WATCHDOG_H

#include <stdint.h>

// cppcheck-suppress misra-c2012-2.3 ; used in driver implementations
// cppcheck-suppress misra-c2012-2.4 ; used in driver implementations
typedef struct simple_watchdog_state_t {
  uint32_t fault;
  uint32_t last_ts;
  uint32_t threshold;
} simple_watchdog_state_t;

void simple_watchdog_kick(void);
void simple_watchdog_init(uint32_t fault, uint32_t threshold);

#endif
