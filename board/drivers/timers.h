#pragma once

static void timer_init(TIM_TypeDef *TIM, int psc) {
  register_set(&(TIM->PSC), (psc-1), 0xFFFFU);
  register_set(&(TIM->DIER), TIM_DIER_UIE, 0x5F5FU);
  register_set(&(TIM->CR1), TIM_CR1_CEN, 0x3FU);
  TIM->SR = 0;
}

void microsecond_timer_init(void) {
  register_set(&(MICROSECOND_TIMER->PSC), (APB1_TIMER_FREQ - 1U), 0xFFFFU);
  register_set(&(MICROSECOND_TIMER->CR1), TIM_CR1_CEN, 0x3FU);
  MICROSECOND_TIMER->EGR = TIM_EGR_UG;
}

uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}

// one timer measured against another, both would have to drift the same way to go unnoticed
void timer_check(timer_check_t *check, uint32_t expected_us) {
  uint32_t now = microsecond_timer_get();
  if (check->last_ts != 0U) {
    uint32_t dt = get_ts_elapsed(now, check->last_ts);
    uint32_t tol = expected_us / 20U;
    if ((dt < (expected_us - tol)) || (dt > (expected_us + tol))) {
      check->mismatches += 1U;
      if (check->mismatches >= 2U) {
        fault_occurred(FAULT_TIMER);
      }
    } else {
      check->mismatches = 0U;
    }
  }
  check->last_ts = now;
}

void interrupt_timer_init(void) {
  enable_interrupt_timer();
  REGISTER_INTERRUPT(INTERRUPT_TIMER_IRQ, interrupt_timer_handler, 2U, FAULT_INTERRUPT_RATE_INTERRUPTS)
  register_set(&(INTERRUPT_TIMER->PSC), ((uint16_t)(15.25*APB1_TIMER_FREQ)-1U), 0xFFFFU);
  register_set(&(INTERRUPT_TIMER->DIER), TIM_DIER_UIE, 0x5F5FU);
  register_set(&(INTERRUPT_TIMER->CR1), TIM_CR1_CEN, 0x3FU);
  INTERRUPT_TIMER->SR = 0;
  nvic_enable(INTERRUPT_TIMER_IRQ);
}

void tick_timer_init(void) {
  timer_init(TICK_TIMER, (uint16_t)((15.25*APB2_TIMER_FREQ)/8U));
  nvic_enable(TICK_TIMER_IRQ);
}
