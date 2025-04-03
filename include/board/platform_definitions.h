/* Platform includes that provide things that a lot drivers need, e.g.:
  __enable_irq
  __disable_irq
  GPIO_TypeDef
  TIM_TypeDef
  IRQn_Type
  USART_TypeDef
  CAN_TypeDef
*/
#pragma once
#define CAN_INIT_TIMEOUT_MS 500U
#ifdef STM32H7
  #include "stm32h7/stm32h7_platform_definitions.h"
#elif defined(STM32F4)
  #include "stm32f4/stm32f4_platform_definitions.h"
#else
  #include "fake_stm.h"
#endif
