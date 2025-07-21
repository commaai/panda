#pragma once

#include "stm32f4xx.h"
#include <stdint.h>

// Forward declarations to avoid circular inclusion
struct fan_state_t;
extern void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
extern void register_set_bits(volatile uint32_t *addr, uint32_t val);
extern void pwm_init(TIM_TypeDef *TIM, uint8_t channel);

// Function declarations
void llfan_init(void);
