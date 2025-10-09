// ///////////////////////// //
// Body board v2 (STM32H7)  //
// ///////////////////////// //

void board_body_init(void) {
  // Add body-specific initialization here
}

void board_body_init_bootloader(void) {
  // Bootloader initialization if needed
}

void board_body_tick(void) {
  // Called at 1Hz - add periodic tasks here
}

void board_body_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  UNUSED(transceiver);
  UNUSED(enabled);
  // Add CAN transceiver control here when needed
}

void board_body_set_can_mode(uint8_t mode) {
  UNUSED(mode);
  // Add CAN mode switching here when needed
}

uint32_t board_body_read_voltage_mV(void) {
  // Add voltage reading here when needed
  return 0U;
}

uint32_t board_body_read_current_mA(void) {
  // Add current reading here when needed
  return 0U;
}

board board_body = {
  .led_GPIO = {GPIOC, GPIOC, GPIOC},
  .led_pin = {7, 7, 7},
  .led_pwm_channels = {0, 0, 0},
  .avdd_mV = 3300U,
  .init = board_body_init,
  .init_bootloader = board_body_init_bootloader,
  .board_tick = board_body_tick,
  .enable_can_transceiver = board_body_enable_can_transceiver,
  .set_can_mode = board_body_set_can_mode,
  .read_voltage_mV = board_body_read_voltage_mV,
  .read_current_mA = board_body_read_current_mA,
  .has_spi = false,
  .has_fan = false,
};
