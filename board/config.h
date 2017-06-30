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

#ifdef PANDA
  #define LED_RED 9
  #define LED_GREEN 7
  #define LED_BLUE 6
#else
  #define LED_RED 10
  #define LED_GREEN 11
  #define LED_BLUE -1
#endif

#define USB_VID 0xbbaa
#define USB_PID 0xddcc

#define FREQ 24000000

// 500 khz
#define CAN_DEFAULT_BITRATE 500000

#define FIFO_SIZE 0x100

#define NULL ((void*)0)

#define PANDA_REV_AB 0
#define PANDA_REV_C 1

#endif
