#pragma once

#include "board/declarations.h"

#include "board/can.h"
#include "board/comms_definitions.h"

#ifndef BOOTSTUB
  #include "board/main_definitions.h"
#else
  #include "board/bootstub_declarations.h"
#endif

#include "board/libc.h"
#include "board/sys/critical.h"
#include "board/sys/faults.h"
#include "board/utils.h"

#include "board/drivers/registers.h"
#include "board/drivers/interrupts.h"

#ifdef BOOTSTUB
uart_ring uart_ring_som_debug;
#endif
#include "board/drivers/gpio.h"
#include "board/stm32h7/peripherals.h"
#include "board/stm32h7/interrupt_handlers.h"
#include "board/drivers/timers.h"

#if !defined(BOOTSTUB)
  #include "board/drivers/uart.h"
  #include "board/stm32h7/lluart.h"
#endif

#include "board/stm32h7/lladc.h"

#ifdef PANDA_JUNGLE
#include "board/jungle/stm32h7/board.h"
#elif defined(PANDA_BODY)
#include "board/body/stm32h7/board.h"
#else
#include "board/stm32h7/board.h"
#endif
#include "board/stm32h7/clock.h"
#if !defined(PANDA_BODY) && !defined(PANDA_JUNGLE)
#include "board/stm32h7/lli2c.h"
#endif

#ifdef BOOTSTUB
  #include "board/stm32h7/llflash.h"
#else
  #include "board/stm32h7/llfdcan.h"
#endif

#include "board/stm32h7/llusb.h"

#include "board/drivers/spi.h"
#include "board/stm32h7/llspi.h"

void early_gpio_float(void) {
  RCC->AHB4ENR = RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN | RCC_AHB4ENR_GPIOGEN | RCC_AHB4ENR_GPIOHEN;
  GPIOA->MODER = 0xAB000000U; GPIOB->MODER = 0; GPIOC->MODER = 0; GPIOD->MODER = 0; GPIOE->MODER = 0; GPIOF->MODER = 0; GPIOG->MODER = 0; GPIOH->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0; GPIOD->ODR = 0; GPIOE->ODR = 0; GPIOF->ODR = 0; GPIOG->ODR = 0; GPIOH->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0; GPIOD->PUPDR = 0; GPIOE->PUPDR = 0; GPIOF->PUPDR = 0; GPIOG->PUPDR = 0; GPIOH->PUPDR = 0;
}
