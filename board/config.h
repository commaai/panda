#ifndef PANDA_CONFIG_H
#define PANDA_CONFIG_H

//#define DEBUG
//#define DEBUG_USB
//#define CAN_LOOPBACK_MODE

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

#endif
