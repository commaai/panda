uint16_t fan_tach_counter = 0U;
uint16_t fan_rpm = 0U;
uint16_t fan_target_rpm = 0U;

#define FAN_MAX_RPM 6500U

#define FAN_I 0.001f
float fan_error_integral = 0.0f;

void fan_set_power(uint8_t percentage){
  fan_target_rpm = ((FAN_MAX_RPM * MIN(100U, MAX(0U, percentage))) / 100U);
}

// Call this at 8Hz
void fan_tick(void){
  // 4 interrupts per rotation
  uint16_t fan_rpm_fast = fan_tach_counter * (60U * 8U / 4U);
  fan_tach_counter = 0U;
  fan_rpm = (fan_rpm_fast + (3U * fan_rpm)) / 4U;

  // Update controller
  float feedforward = (fan_target_rpm * 100.0f) / FAN_MAX_RPM;
  float error = fan_target_rpm - fan_rpm_fast;

  fan_error_integral += FAN_I * error;
  fan_error_integral = MIN(70.0f, MAX(-70.0f, fan_error_integral));
  
  uint8_t output = MIN(100U, MAX(0U, feedforward + fan_error_integral));
  pwm_set(TIM3, 3, output);
}
