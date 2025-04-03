#pragma once
#include "stm32h7_platform_definitions.h"

#include "stm32h7/peripherals.h"
#include "stm32h7/interrupt_handlers.h"

#if !defined(BOOTSTUB)
  #include "stm32h7/lluart.h"
#endif

#ifdef PANDA_JUNGLE
  #include "board/jungle/stm32h7/board.h"
#else
  #include "board/stm32h7/board.h"
#endif

#include "stm32h7/clock.h"

#ifdef BOOTSTUB
  #include "stm32h7/llflash.h"
#else
  #include "stm32h7/llfdcan.h"
#endif

#include "stm32h7/llusb.h"
#include "stm32h7/llspi.h"

void early_gpio_float(void) {
  RCC->AHB4ENR = RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN | RCC_AHB4ENR_GPIOGEN | RCC_AHB4ENR_GPIOHEN;
  GPIOA->MODER = 0xAB000000U; GPIOB->MODER = 0; GPIOC->MODER = 0; GPIOD->MODER = 0; GPIOE->MODER = 0; GPIOF->MODER = 0; GPIOG->MODER = 0; GPIOH->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0; GPIOD->ODR = 0; GPIOE->ODR = 0; GPIOF->ODR = 0; GPIOG->ODR = 0; GPIOH->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0; GPIOD->PUPDR = 0; GPIOE->PUPDR = 0; GPIOF->PUPDR = 0; GPIOG->PUPDR = 0; GPIOH->PUPDR = 0;
}
