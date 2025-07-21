#pragma once

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

// llfan_init keeps separate declaration as it's platform-specific
void llfan_init(void);
// Other function declarations removed - now static inline in fan.h
