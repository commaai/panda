uint16_t fan_tach_counter = 0U;
uint16_t fan_rpm = 0U;
uint16_t fan_rpm_fast = 0U;

void fan_set_power(uint8_t percentage){
  pwm_set(TIM3, 3, percentage);
}

// Call this at 8Hz
void fan_tick(void){
  // 4 interrupts per rotation
  fan_rpm_fast = fan_tach_counter * 15U * 8U;
  puth(fan_rpm_fast); puts("\n");
  fan_tach_counter = 0U;

  // Calculate low-pass filtered version
  fan_rpm = (fan_rpm_fast + (3U * fan_rpm)) / 4U;
}
