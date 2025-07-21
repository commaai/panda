#pragma once

#include <stdbool.h>

#ifdef STM32H7
  #include "stm32h7xx.h"
#elif defined(STM32F4)
  #include "stm32f4xx.h"
#endif

#define CODEC_I2C_ADDR 0x10

// Function declarations
void fake_siren_init(void);
void fake_siren_codec_enable(bool enabled);
void fake_siren_set(bool enabled);
