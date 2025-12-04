#pragma once

#include "board/config.h"

#include "board/board_forward.h"
#include "board/boards/boot_state.h"

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
} board;


#ifdef PANDA
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_RED_PANDA 7U
#define HW_TYPE_TRES 9U
#define HW_TYPE_CUATRO 10U

// CAN modes
#define CAN_MODE_NORMAL 0U
#define CAN_MODE_OBD_CAN2 1U

#elif defined(PANDA_JUNGLE)
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_V2 2U

// CAN modes
#define CAN_MODE_NORMAL 0U
#define CAN_MODE_OBD_CAN2 3U

// Harness states
#define HARNESS_ORIENTATION_NONE 0U
#define HARNESS_ORIENTATION_1 1U
#define HARNESS_ORIENTATION_2 2U

#define SBU1 0U
#define SBU2 1U

#elif defined(PANDA_JUNGLE)

#elif defined(PANDA_BODY)
#elif defined(LIB_PANDA)
// NO IDEA BOI
#else
#error FUCK YOU
#endif