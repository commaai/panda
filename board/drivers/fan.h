#ifndef PANDA_DRIVERS_FAN_H
#define PANDA_DRIVERS_FAN_H

#include <stdint.h>

struct fan_state_t {
  uint16_t tach_counter;
  uint16_t rpm;
  uint8_t power;
  float error_integral;
  uint8_t cooldown_counter;
};

extern struct fan_state_t fan_state;

void fan_set_power(uint8_t percentage);
void fan_init(void);
void fan_tick(void);

#endif
