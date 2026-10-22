#pragma once

static void timer_init(TIM_TypeDef *TIM, int psc) {
  register_set(&(TIM->PSC), (psc-1), 0xFFFFU);
  register_set(&(TIM->DIER), TIM_DIER_UIE, 0x5F5FU);
  register_set(&(TIM->CR1), TIM_CR1_CEN, 0x3FU);
  TIM->SR = 0;
}

// drop an update that pended before init, read back so the clear lands first
static void timer_irq_enable(TIM_TypeDef *TIM, IRQn_Type irq) {
  TIM->SR = 0;
  (void)TIM->SR;
  NVIC_ClearPendingIRQ(irq);
  NVIC_EnableIRQ(irq);
}

void microsecond_timer_init(void) {
  MICROSECOND_TIMER->PSC = (APB1_TIMER_FREQ - 1U);
  MICROSECOND_TIMER->CR1 = TIM_CR1_CEN;
  MICROSECOND_TIMER->EGR = TIM_EGR_UG;
}

uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}

void interrupt_timer_init(void) {
  enable_interrupt_timer();
  REGISTER_INTERRUPT(INTERRUPT_TIMER_IRQ, interrupt_timer_handler, 2U, FAULT_INTERRUPT_RATE_INTERRUPTS)
  register_set(&(INTERRUPT_TIMER->PSC), ((uint16_t)(15.25*APB1_TIMER_FREQ)-1U), 0xFFFFU);
  register_set(&(INTERRUPT_TIMER->DIER), TIM_DIER_UIE, 0x5F5FU);
  register_set(&(INTERRUPT_TIMER->CR1), TIM_CR1_CEN, 0x3FU);
  // load the prescaler now, so the first window covers all of boot
  INTERRUPT_TIMER->EGR = TIM_EGR_UG;
  timer_irq_enable(INTERRUPT_TIMER, INTERRUPT_TIMER_IRQ);
}

void tick_timer_init(void) {
  timer_init(TICK_TIMER, (uint16_t)((15.25*APB2_TIMER_FREQ)/8U));
  timer_irq_enable(TICK_TIMER, TICK_TIMER_IRQ);
}
