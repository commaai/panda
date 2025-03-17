#pragma once
// Platform includes that provide things that a lot drivers need, e.g.:
//  __enable_irq
//  __disable_irq
//  GPIO_TypeDef
//  TIM_TypeDef
//  IRQn_Type
//  USART_TypeDef
//  CAN_TypeDef
#ifdef STM32H7
  #include "stm32h7/stm32h7_platform_definitions.h"
#elif defined(STM32F4)
  #include "stm32f4/stm32f4_platform_definitions.h"
#endif
