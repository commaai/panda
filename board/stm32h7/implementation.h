#pragma once

// Common implementation includes for the single-translation-unit firmware build.
#include "board/config.h"

#ifndef BOOTSTUB
  #include "board/main_definitions.h"
#else
  #include "board/bootstub.h"
#endif

#include "board/libc.h"
#include "board/crc.h"
#include "board/sys/critical.h"
#include "board/sys/faults.h"

#include "board/drivers/registers.h"
#include "board/drivers/interrupts.h"

#include "board/drivers/gpio.h"
#include "board/stm32h7/peripherals.h"
#include "board/stm32h7/interrupt_handlers.h"
#include "board/drivers/timers.h"

#if !defined(BOOTSTUB)
  #include "board/drivers/uart.h"
  #include "board/stm32h7/lluart.h"
#endif

#ifdef PANDA_JUNGLE
#include "board/jungle/stm32h7/board.h"
#elif defined(PANDA_BODY)
#include "board/body/stm32h7/board.h"
#else
#include "board/stm32h7/board.h"
#endif
#include "board/stm32h7/clock.h"

#ifdef BOOTSTUB
  #include "board/stm32h7/llflash.h"
#else
  #include "board/stm32h7/llfdcan.h"
#endif

#include "board/stm32h7/llusb.h"

#include "board/drivers/spi.h"
#include "board/stm32h7/llspi.h"
