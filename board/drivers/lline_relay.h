#ifdef PANDA

/* Conrol a relay connected to l-line pin */

// 100 us high, 5 ms low

volatile int turn_on_relay = 1;
volatile int on_cycles = 90;

void TIM5_IRQHandler(void) {
  if (TIM5->SR & TIM_SR_UIF) {
    on_cycles--;
    if (on_cycles) {
      if (turn_on_relay) {
        set_gpio_output(GPIOC, 10, 0);
      }
    }
    else {
      set_gpio_output(GPIOC, 10, 1);
      on_cycles = 90;
    }
  }
  TIM5->ARR = 30-1;
  TIM5->SR = 0;
}

void lline_relay_init (void) {
  set_gpio_output(GPIOC, 10, 1);

  // setup
  TIM5->PSC = 48-1; // tick on 1 us
  TIM5->CR1 = TIM_CR1_CEN;   // enable
  TIM5->ARR = 50-1;         // 100 us

  // in case it's disabled
  NVIC_EnableIRQ(TIM5_IRQn);

  // run the interrupt
  TIM5->DIER = TIM_DIER_UIE; // update interrupt
  TIM5->SR = 0;
}

void set_lline_output(int to_set) {
  turn_on_relay = to_set;
}

#endif
