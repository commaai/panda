#ifndef PANDA_CONFIG_H
#define PANDA_CONFIG_H

//#define DEBUG
//#define DEBUG_UART
//#define DEBUG_USB
//#define DEBUG_SPI
//#define DEBUG_FAULTS

#ifdef STM32H7
  #define PANDA
  #include "stm32h7xx.h"
  #define CORE_FREQ 240U // 240Mhz
#elif STM32F4
  #define PANDA
  #include "stm32f4xx.h"
  #define CORE_FREQ 96U // 96Mhz
#else
  #include "stm32f2xx.h"
  #define CORE_FREQ 96U // 96Mhz
#endif

#define USB_VID 0xbbaaU

#ifdef BOOTSTUB
#define USB_PID 0xddeeU
#else
#define USB_PID 0xddffU //REDEBUG (to be able to test with *;D* )
#endif

#include <stdbool.h>
#define NULL ((void*)0)
#define COMPILE_TIME_ASSERT(pred) ((void)sizeof(char[1 - (2 * ((int)(!(pred))))]))

#define MIN(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   (_a < _b) ? _a : _b; })

#define MAX(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   (_a > _b) ? _a : _b; })

#define ABS(a) \
 ({ __typeof__ (a) _a = (a); \
   (_a > 0) ? _a : (-_a); })

#define MAX_RESP_LEN 0x40U

// Around (1Mbps / 8 bits/byte / 12 bytes per message)
#define CAN_INTERRUPT_RATE 12000U

#endif

