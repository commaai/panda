struct fan_state_t {
  uint16_t tach_counter;
  uint16_t rpm;
  uint16_t target_rpm;
  uint8_t power;
  float error_integral;
} fan_state_t;
struct fan_state_t fan_state;

#define FAN_I 0.001f

void fan_set_power(uint8_t percentage){
  fan_state.target_rpm = ((current_board->fan_max_rpm * MIN(100U, MAX(0U, percentage))) / 100U);
  current_board->set_fan_enabled(percentage > 0U);
}

// Call this at 8Hz
void fan_tick(void){
  if (current_board->fan_max_rpm > 0U) {
    // 4 interrupts per rotation
    uint16_t fan_rpm_fast = fan_state.tach_counter * (60U * 8U / 4U);
    fan_state.tach_counter = 0U;
    fan_state.rpm = (fan_rpm_fast + (3U * fan_state.rpm)) / 4U;

    // Update controller
    float feedforward = (fan_state.target_rpm * 100.0f) / current_board->fan_max_rpm;
    float error = fan_state.target_rpm - fan_rpm_fast;

    fan_state.error_integral += FAN_I * error;
    fan_state.error_integral = MIN(70.0f, MAX(-70.0f, fan_state.error_integral));

    fan_state.power = MIN(100U, MAX(0U, feedforward + fan_state.error_integral));
    pwm_set(TIM3, 3, fan_state.power);
  }
}
