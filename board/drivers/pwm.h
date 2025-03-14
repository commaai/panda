#pragma once

// TODO: Implement for 32-bit timers

void pwm_init(TIM_TypeDef *TIM, uint8_t channel);

void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
