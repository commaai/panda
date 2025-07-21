#ifdef STM32H7
  #include "stm32h7xx.h"
#elif defined(STM32F4)
  #include "stm32f4xx.h"
#endif

#define PWM_COUNTER_OVERFLOW 2000U // To get ~50kHz

// TODO: Implement for 32-bit timers

void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
