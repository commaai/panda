#pragma once

#include <stdint.h>

#define PWM_COUNTER_OVERFLOW 4800U

void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
