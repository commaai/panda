#pragma once

#include <stdbool.h>

//#define DEBUG
//#define DEBUG_UART
//#define DEBUG_USB
//#define DEBUG_SPI
//#define DEBUG_FAULTS
//#define DEBUG_COMMS
//#define DEBUG_FAN

#define CAN_INIT_TIMEOUT_MS 500U
#define USBPACKET_MAX_SIZE 0x40U
#define MAX_CAN_MSGS_PER_USB_BULK_TRANSFER 51U
#define MAX_CAN_MSGS_PER_SPI_BULK_TRANSFER 170U

// USB definitions
#define USB_VID 0x3801U

#ifdef PANDA_JUNGLE
  #ifdef BOOTSTUB
    #define USB_PID 0xDDEFU
  #else
    #define USB_PID 0xDDCFU
  #endif
#else
  #ifdef BOOTSTUB
    #define USB_PID 0xDDEEU
  #else
    #define USB_PID 0xDDCCU
  #endif
#endif

#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_V2 2U
#define HW_TYPE_RED_PANDA 7U
#define HW_TYPE_TRES 9U
#define HW_TYPE_CUATRO 10U

#ifdef PANDA

// CAN modes
#define CAN_MODE_NORMAL 0U
#define CAN_MODE_OBD_CAN2 1U

#elif defined(PANDA_JUNGLE)

// CAN modes
#define CAN_MODE_NORMAL 0U
#define CAN_MODE_OBD_CAN2 3U

// Harness states
#define HARNESS_ORIENTATION_NONE 0U
#define HARNESS_ORIENTATION_1 1U
#define HARNESS_ORIENTATION_2 2U

#define SBU1 0U
#define SBU2 1U

#elif defined(PANDA_BODY)
#elif defined(LIB_PANDA)
#else
#error Unknown board type
#endif

// platform includes
#ifdef STM32H7
  #include "board/stm32h7/stm32h7_config.h"
#else
  // building for tests
  #include "fake_stm.h"
#endif

void detect_board_type(void);