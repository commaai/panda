#pragma once

#include <stdint.h>

#include "board/config.h"
#include "board/board_struct.h"

// #include "board_forward.h"

// struct board {
//   GPIO_TypeDef * const led_GPIO[3];
//   const uint8_t led_pin[3];
//   const uint8_t led_pwm_channels[3]; // leave at 0 to disable PWM
//   const uint16_t avdd_mV;
//   board_init init;
//   board_board_tick board_tick;
//   board_get_button get_button;
//   board_init_bootloader init_bootloader;
//   board_set_panda_power set_panda_power;
//   board_set_panda_individual_power set_panda_individual_power;
//   board_set_ignition set_ignition;
//   board_set_individual_ignition set_individual_ignition;
//   board_set_harness_orientation set_harness_orientation;
//   board_set_can_mode set_can_mode;
//   board_enable_can_transceiver enable_can_transceiver;
//   board_enable_header_pin enable_header_pin;
//   board_get_channel_power get_channel_power;
//   board_get_sbu_mV get_sbu_mV;

//   // TODO: shouldn't need these
//   bool has_spi;
// };

// ******************* Definitions ********************

// ********************* Globals **********************
extern uint8_t harness_orientation;
extern uint8_t can_mode;
extern uint8_t ignition;
