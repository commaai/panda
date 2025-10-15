
#include "board/body/boards/motor_control.h"

void board_body_init(void) {
  motor_init();
  motor_encoder_init();
  motor_speed_controller_init();
  motor_stop(1);
  motor_stop(2);
  motor_encoder_reset(1);
  motor_encoder_reset(2);
  
}

void board_body_init_bootloader(void) {
}

void board_body_tick(void) {
}

void board_body_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  UNUSED(transceiver);
  set_gpio_output(GPIOD, 2U, enabled ? 1 : 0);
}

void board_body_set_can_mode(uint8_t mode) {
  UNUSED(mode);
}

uint32_t board_body_read_voltage_mV(void) {
  return 0U;
}

uint32_t board_body_read_current_mA(void) {
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
