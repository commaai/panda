#pragma once

#include "board/drivers/register_declarations.h"

#include "stm32h7xx.h"

#include <stdint.h>
#include <stdbool.h>
#include "board/body/bldc/bldc_defs.h"

// ******************** Prototypes ********************
typedef void (*board_init)(void);
typedef void (*board_enable_can_transceiver)(uint8_t transceiver, bool enabled);

struct board {
  const tracked_gpio * const led_GPIO[3];
  const uint8_t led_pin[3];
  const uint8_t led_pwm_channels[3]; // leave at 0 to disable PWM
  board_init init;
  const bool has_spi;
};

// ******************* Definitions ********************
#define HW_TYPE_BODY 0xB1U

// Pin definitions
// CAN
#define CAN_TX_PORT (&tracked_GPIOD)
#define CAN_TX_PIN  1
#define CAN_RX_PORT (&tracked_GPIOD)
#define CAN_RX_PIN  0
#define CAN_TRANSCEIVER_EN_PORT (&tracked_GPIOD)
#define CAN_TRANSCEIVER_EN_PIN  12

// Ignition and charging detection
#define IGNITION_SW_PORT         (&tracked_GPIOC)
#define IGNITION_SW_PIN          15
#define CHARGING_DETECT_PORT     (&tracked_GPIOC)
#define CHARGING_DETECT_PIN      13

// Dotstar LED
#define DOTSTAR_CLK_PORT  (&tracked_GPIOB)
#define DOTSTAR_CLK_PIN   3
#define DOTSTAR_DATA_PORT (&tracked_GPIOB)
#define DOTSTAR_DATA_PIN  5

// Mici Power On
#define OBDC_POWER_ON_PORT (&tracked_GPIOB)
#define OBDC_POWER_ON_PIN  12

// GPU Power On
#define GPU_POWER_ON_PORT (&tracked_GPIOD)
#define GPU_POWER_ON_PIN  8

// Ignition On
#define OBDC_IGNITION_ON_PORT (&tracked_GPIOB)
#define OBDC_IGNITION_ON_PIN  11

extern struct board board_body;
