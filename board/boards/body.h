#pragma once

#include "board_declarations.h"
#include "unused_funcs.h"

static void body_init(void) {
}

static void body_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  (void)transceiver;
  (void)enabled;
}

static void body_set_can_mode(uint8_t mode) {
  (void)mode;
}

static uint32_t body_read_voltage_mV(void) {
  return 0U;
}

board board_body = {
  .harness_config = 0,
  .led_GPIO = {GPIOC, GPIOC, GPIOC},
  .led_pin = {7U, 7U, 7U},
  .led_pwm_channels = {0U, 0U, 0U},
  .has_spi = false,
  .has_fan = false,
  .avdd_mV = 0U,
  .fan_enable_cooldown_time = 0U,
  .init = body_init,
  .init_bootloader = unused_init_bootloader,
  .enable_can_transceiver = body_enable_can_transceiver,
  .set_can_mode = body_set_can_mode,
  .read_voltage_mV = body_read_voltage_mV,
  .read_current_mA = unused_read_current,
  .set_ir_power = unused_set_ir_power,
  .set_fan_enabled = unused_set_fan_enabled,
  .set_siren = unused_set_siren,
  .set_bootkick = unused_set_bootkick,
  .read_som_gpio = unused_read_som_gpio,
  .set_amp_enabled = unused_set_amp_enabled
};
