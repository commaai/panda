#pragma once

#include <stdint.h>
#include <stdbool.h>

// ******************** Prototypes ********************
typedef void (*board_init)(void);
typedef void (*board_init_bootloader)(void);
typedef void (*board_enable_can_transceiver)(uint8_t transceiver, bool enabled);
typedef void (*board_set_can_mode)(uint8_t mode);

struct board {
  GPIO_TypeDef * const led_GPIO[3];
  const uint8_t led_pin[3];
  const uint8_t led_pwm_channels[3]; // leave at 0 to disable PWM
  board_init init;
  board_init_bootloader init_bootloader;
  board_enable_can_transceiver enable_can_transceiver;
  board_set_can_mode set_can_mode;
  const bool has_spi;
};

// ******************* Definitions ********************
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_BODY 0xB1U

// CAN modes
#define CAN_MODE_NORMAL 0U

// ********************* Globals **********************
uint8_t can_mode = CAN_MODE_NORMAL;
