#pragma once

#include "board/board_struct.h"

// #pragma once

// #include <stdint.h>
// #include <stdbool.h>

// #include "board/board_forward.h"

// // ******************** Prototypes ********************
// typedef void (*board_init)(void);
// typedef void (*board_init_bootloader)(void);
// typedef void (*board_enable_can_transceiver)(uint8_t transceiver, bool enabled);

// typedef struct board {
//   struct harness_configuration *harness_config;
//   GPIO_TypeDef * const led_GPIO[3];
//   const uint8_t led_pin[3];
//   const uint8_t led_pwm_channels[3]; // leave at 0 to disable PWM
//   const bool has_spi;
//   const bool has_fan;
//   const uint16_t avdd_mV;
//   const uint8_t fan_enable_cooldown_time;
//   board_init init;
//   board_init_bootloader init_bootloader;
//   board_enable_can_transceiver enable_can_transceiver;
//   board_set_can_mode set_can_mode;
//   board_read_voltage_mV read_voltage_mV;
//   board_read_current_mA read_current_mA;
//   board_set_ir_power set_ir_power;
//   board_set_fan_enabled set_fan_enabled;
//   board_set_siren set_siren;
//   board_set_bootkick set_bootkick;
//   board_read_som_gpio read_som_gpio;
//   board_set_amp_enabled set_amp_enabled;
// } board;



// // ******************* Definitions ********************
// #define HW_TYPE_BODY 0xB1U
