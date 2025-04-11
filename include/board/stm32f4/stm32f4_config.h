#pragma once
#include "platform_definitions.h"
#include "can.h"
#include "comms_definitions.h"

#ifndef BOOTSTUB
  #include "main_definitions.h"
#else
  #include "bootstub_declarations.h"
#endif

#include "libc.h"
#include "critical.h"
#include "faults.h"
#include "utils.h"

#include "drivers/registers.h"
#include "drivers/interrupts.h"
#include "drivers/gpio.h"
#include "stm32f4/peripherals.h"
#include "stm32f4/interrupt_handlers.h"
#include "drivers/timers.h"
#include "stm32f4/board.h"
#include "stm32f4/clock.h"
#include "drivers/watchdog.h"

#include "drivers/spi.h"
#include "stm32f4/llspi.h"

#if !defined(BOOTSTUB)
  #include "drivers/uart.h"
  #include "stm32f4/lluart.h"
#endif

#ifdef BOOTSTUB
  #include "stm32f4/llflash.h"
#else
  #include "stm32f4/llbxcan.h"
#endif

#include "stm32f4/llusb.h"

static inline void early_gpio_float(void) {
  RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

  GPIOB->MODER = 0; GPIOC->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0;
}
