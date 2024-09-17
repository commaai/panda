#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct simple_watchdog_state_t {
  uint32_t fault;
  uint32_t last_ts;
  uint32_t threshold;
} simple_watchdog_state_t;

extern simple_watchdog_state_t wd_state;

void simple_watchdog_kick(void);
void simple_watchdog_init(uint32_t fault, uint32_t threshold);
