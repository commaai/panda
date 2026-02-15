#pragma once

#include <stdint.h>

struct fan_state_t {
  uint16_t tach_counter;
  uint16_t rpm;
  uint8_t power;
  float error_integral;
  uint8_t cooldown_counter;
};

// External variables
extern struct fan_state_t fan_state;

// Function declarations
void fan_set_power(uint8_t percentage);
void llfan_init(void);
void fan_init(void);
void fan_tick(void);
