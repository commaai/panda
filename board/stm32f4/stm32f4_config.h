#pragma once
#include "stm32f4_platform_definitions.h"

#include "stm32f4/peripherals.h"
#include "stm32f4/interrupt_handlers.h"
#include "stm32f4/clock.h"

#ifdef PANDA_JUNGLE
  #include "board/jungle/stm32f4/board.h"
#else
  #include "board/stm32f4/board.h"
#endif

#include "stm32f4/llspi.h"

#if !defined(BOOTSTUB)
  #include "stm32f4/lluart.h"
#endif

#ifdef BOOTSTUB
  #include "stm32f4/llflash.h"
#else
  #include "stm32f4/llbxcan.h"
#endif

#include "stm32f4/llusb.h"

void early_gpio_float(void) {
  RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

  GPIOB->MODER = 0; GPIOC->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0;
}
