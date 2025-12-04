#pragma once

#include <stdint.h>

#include "utils.h"
// #include "stm32h7/inc/stm32h7xx.h"
#include "config.h"

// ******************** Prototypes ********************



typedef struct harness_configuration harness_configuration;
void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);

// ********************* Globals **********************

#include "globals.h"