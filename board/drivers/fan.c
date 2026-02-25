#include "board/config.h"

struct fan_state_t fan_state;

static const uint8_t FAN_TICK_FREQ = 8U;

void fan_set_power(uint8_t percentage) {
  if (percentage > 0U) {
    fan_state.power = CLAMP(percentage, 20U, 100U);
  } else {
    fan_state.power = 0U;
  }
}

void fan_init(void) {
  fan_state.cooldown_counter = current_board->fan_enable_cooldown_time * FAN_TICK_FREQ;
  llfan_init();
}

void fan_tick(void) {
  if (current_board->has_fan) {
    uint16_t fan_rpm_fast = fan_state.tach_counter * (60U * FAN_TICK_FREQ / 4U);
    fan_state.tach_counter = 0U;
    fan_state.rpm = (fan_rpm_fast + (3U * fan_state.rpm)) / 4U;

    #ifdef DEBUG_FAN
      puth(fan_state.target_rpm);
      print(" "); puth(fan_rpm_fast);
      print(" "); puth(fan_state.power);
      print("\n");
    #endif

    if (fan_state.power > 0U) {
      fan_state.cooldown_counter = current_board->fan_enable_cooldown_time * FAN_TICK_FREQ;
    } else {
      if (fan_state.cooldown_counter > 0U) {
        fan_state.cooldown_counter--;
      }
    }

    pwm_set(TIM3, 3, fan_state.power);
    current_board->set_fan_enabled((fan_state.power > 0U) || (fan_state.cooldown_counter > 0U));
  }
}
