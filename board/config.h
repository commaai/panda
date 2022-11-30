#ifndef PANDA_CONFIG_H
#define PANDA_CONFIG_H

//#define DEBUG
//#define DEBUG_UART
//#define DEBUG_USB
//#define DEBUG_SPI
//#define DEBUG_FAULTS
//#define DEBUG_COMMS

#define DEEPSLEEP_WAKEUP_DELAY 3U

#define COMPILE_TIME_ASSERT(pred) ((void)sizeof(char[1 - (2 * ((int)(!(pred))))]))

#include <stdbool.h>

// USB definitions
#define USB_VID 0xBBAAU

#ifdef BOOTSTUB
  #define USB_PID 0xDDEEU
#else
  #define USB_PID 0xDDCCU
#endif

#define USBPACKET_MAX_SIZE 0x40U

#define MAX_CAN_MSGS_PER_BULK_TRANSFER 51U
#define MAX_EP1_CHUNK_PER_BULK_TRANSFER 16256U // max data stream chunk in bytes, shouldn't be higher than 16320 or counter will overflow

#define CAN_INIT_TIMEOUT_MS 500U

#ifdef STM32H7
  #include "stm32h7/stm32h7_config.h"
#elif defined(STM32F2) || defined(STM32F4)
  #include "stm32fx/stm32fx_config.h"
#else
  // building for tests
  #include "fake_stm.h"
#endif

#endif
