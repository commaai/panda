#ifndef PANDA_REV_H
#define PANDA_REV_H

enum rev {
  REV_A,
  REV_B,
  REV_C
};

#ifdef STM32F4
  #define PANDA
#endif

#ifdef PANDA
  #include "stm32f4xx.h"
  #include "stm32f4xx_hal_gpio_ex.h"
#else
  #include "stm32f2xx.h"
  #include "stm32f2xx_hal_gpio_ex.h"
#endif

#ifdef PANDA
  #define ENABLE_CURRENT_SENSOR
  #define ENABLE_SPI
#endif

#ifdef PANDA
  #define LED_RED 9
  #define LED_GREEN 7
  #define LED_BLUE 6
#else
  #define LED_RED 10
  #define LED_GREEN 11
  #define LED_BLUE -1
#endif

#define FREQ 24000000 // 24 mhz

#endif
