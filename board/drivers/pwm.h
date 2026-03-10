#pragma once

#include <stdint.h>
#include "stm32h7/inc/stm32h725xx.h"

#define PWM_COUNTER_OVERFLOW 4800U // To get ~25kHz

void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
