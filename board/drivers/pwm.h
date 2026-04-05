#ifndef PWM_H
#define PWM_H

#include <stdint.h>

#ifdef STM32H7
#include "stm32h7xx.h"
#endif

#define PWM_COUNTER_OVERFLOW 4800U // To get ~25kHz

void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);

#endif
