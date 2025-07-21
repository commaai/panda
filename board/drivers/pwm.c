#include "board/drivers/pwm.h"

#ifndef BOOTSTUB
// Forward declarations for register functions
void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
void register_set_bits(volatile uint32_t *addr, uint32_t val);
#endif

void pwm_init(TIM_TypeDef *TIM, uint8_t channel){
  // Enable timer and auto-reload
#ifdef BOOTSTUB
  TIM->CR1 |= TIM_CR1_CEN | TIM_CR1_ARPE;
#else
  register_set(&(TIM->CR1), TIM_CR1_CEN | TIM_CR1_ARPE, 0x3FU);
#endif

  // Set channel as PWM mode 1 and enable output
  switch(channel){
    case 1U:
#ifdef BOOTSTUB
      TIM->CCMR1 |= (TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE);
      TIM->CCER |= TIM_CCER_CC1E;
#else
      register_set_bits(&(TIM->CCMR1), (TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE));
      register_set_bits(&(TIM->CCER), TIM_CCER_CC1E);
#endif
      break;
    case 2U:
#ifdef BOOTSTUB
      TIM->CCMR1 |= (TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE);
      TIM->CCER |= TIM_CCER_CC2E;
#else
      register_set_bits(&(TIM->CCMR1), (TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE));
      register_set_bits(&(TIM->CCER), TIM_CCER_CC2E);
#endif
      break;
    case 3U:
#ifdef BOOTSTUB
      TIM->CCMR2 |= (TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3PE);
      TIM->CCER |= TIM_CCER_CC3E;
#else
      register_set_bits(&(TIM->CCMR2), (TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3PE));
      register_set_bits(&(TIM->CCER), TIM_CCER_CC3E);
#endif
      break;
    case 4U:
#ifdef BOOTSTUB
      TIM->CCMR2 |= (TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE);
      TIM->CCER |= TIM_CCER_CC4E;
#else
      register_set_bits(&(TIM->CCMR2), (TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE));
      register_set_bits(&(TIM->CCER), TIM_CCER_CC4E);
#endif
      break;
    default:
      break;
  }

  // Set max counter value
#ifdef BOOTSTUB
  TIM->ARR = PWM_COUNTER_OVERFLOW;
#else
  register_set(&(TIM->ARR), PWM_COUNTER_OVERFLOW, 0xFFFFU);
#endif

  // Update registers and clear counter
  TIM->EGR |= TIM_EGR_UG;
}

void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage){
  uint16_t comp_value = (((uint16_t) percentage * PWM_COUNTER_OVERFLOW) / 100U);
  switch(channel){
    case 1U:
#ifdef BOOTSTUB
      TIM->CCR1 = comp_value;
#else
      register_set(&(TIM->CCR1), comp_value, 0xFFFFU);
#endif
      break;
    case 2U:
#ifdef BOOTSTUB
      TIM->CCR2 = comp_value;
#else
      register_set(&(TIM->CCR2), comp_value, 0xFFFFU);
#endif
      break;
    case 3U:
#ifdef BOOTSTUB
      TIM->CCR3 = comp_value;
#else
      register_set(&(TIM->CCR3), comp_value, 0xFFFFU);
#endif
      break;
    case 4U:
#ifdef BOOTSTUB
      TIM->CCR4 = comp_value;
#else
      register_set(&(TIM->CCR4), comp_value, 0xFFFFU);
#endif
      break;
    default:
      break;
  }
}