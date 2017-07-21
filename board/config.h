#ifndef PANDA_CONFIG_H
#define PANDA_CONFIG_H

//#define DEBUG
//#define DEBUG_USB

#ifdef STM32F4
  #define PANDA
  #include "stm32f4xx.h"
#else
  #include "stm32f2xx.h"
#endif

#ifdef PANDA
  #define ENABLE_CURRENT_SENSOR
  #define ENABLE_SPI
#endif

#define USB_VID 0xbbaa
#define USB_PID 0xddcc

#define NULL ((void*)0)
#define COMPILE_TIME_ASSERT(pred) switch(0){case 0:case pred:;}

#endif
