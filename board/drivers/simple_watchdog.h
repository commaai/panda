#pragma once

#include <stdint.h>

typedef struct simple_watchdog_state_t {
  uint32_t fault;
  uint32_t last_ts;
  uint32_t threshold;
} simple_watchdog_state_t;

extern simple_watchdog_state_t wd_state;