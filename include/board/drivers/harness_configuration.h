#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "platform_definitions.h"

typedef struct harness_configuration {
  const bool has_harness;
  GPIO_TypeDef * const GPIO_SBU1;
  GPIO_TypeDef * const GPIO_SBU2;
  GPIO_TypeDef * const GPIO_relay_SBU1;
  GPIO_TypeDef * const GPIO_relay_SBU2;
  const uint8_t pin_SBU1;
  const uint8_t pin_SBU2;
  const uint8_t pin_relay_SBU1;
  const uint8_t pin_relay_SBU2;
  const uint8_t adc_channel_SBU1;
  const uint8_t adc_channel_SBU2;
} harness_configuration;
