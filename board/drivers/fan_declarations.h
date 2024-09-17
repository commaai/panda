#pragma once

#include <stdint.h>
#include <stdbool.h>

struct fan_state_t {
  uint16_t tach_counter;
  uint16_t rpm;
  uint16_t target_rpm;
  uint8_t power;
  float error_integral;
  uint8_t stall_counter;
  uint8_t stall_threshold;
  uint8_t total_stall_count;
  uint8_t cooldown_counter;
};
extern struct fan_state_t fan_state;

extern const float FAN_I;

extern const uint8_t FAN_TICK_FREQ;
extern const uint8_t FAN_STALL_THRESHOLD_MIN;
extern const uint8_t FAN_STALL_THRESHOLD_MAX;


void fan_set_power(uint8_t percentage);

void llfan_init(void);
void fan_init(void);

// Call this at FAN_TICK_FREQ
void fan_tick(void);
