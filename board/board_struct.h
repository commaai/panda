#pragma once

#include "board/config.h"
#include "board/board_forward.h"
#include "board/boards/boards.h"

typedef struct board {
  struct harness_configuration *harness_config;
  GPIO_TypeDef * const led_GPIO[3];
  const uint8_t led_pin[3];
  const uint8_t led_pwm_channels[3]; // leave at 0 to disable PWM
  const bool has_spi;
  const bool has_fan;
  const uint16_t avdd_mV;
  const uint8_t fan_enable_cooldown_time;
  board_init init;
  board_init_bootloader init_bootloader;
  board_board_tick board_tick;
  board_set_panda_power set_panda_power;
  board_get_button get_button;
  board_enable_can_transceiver enable_can_transceiver;
  board_set_can_mode set_can_mode;
  board_read_voltage_mV read_voltage_mV;
  board_read_current_mA read_current_mA;
  board_set_ir_power set_ir_power;
  board_set_fan_enabled set_fan_enabled;
  board_set_siren set_siren;
  board_set_bootkick set_bootkick;
  board_read_som_gpio read_som_gpio;
  board_set_amp_enabled set_amp_enabled;
  board_set_panda_individual_power set_panda_individual_power;
  board_set_ignition set_ignition;
  board_set_individual_ignition set_individual_ignition;
  board_set_harness_orientation set_harness_orientation;
  board_enable_header_pin enable_header_pin;
  board_get_channel_power get_channel_power;
  board_get_sbu_mV get_sbu_mV;
} board;
