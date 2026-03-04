#ifndef DRIVERS_PWM_H
#define DRIVERS_PWM_H

#include <stdint.h>
#include "stm32fx_def.h"

// TODO: Implement for 32-bit timers

void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);

#endif
