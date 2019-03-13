#ifdef PANDA

/* Conrol a relay connected to l-line pin */

// 30us cycles, 1 high, 90 low

volatile int turn_on_relay = 0;
volatile int on_cycles = 90;

//240 ms timeout
#define LLINE_TIMEOUT_CYCLES 8000
volatile int timeout_cycles = LLINE_TIMEOUT_CYCLES;

void TIM5_IRQHandler(void) {
  if (TIM5->SR & TIM_SR_UIF) {
    on_cycles--;
    timeout_cycles--;
    if (timeout_cycles == 0) {
      turn_on_relay = 0;
    }
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
  set_gpio_mode(GPIOC, 10, MODE_OUTPUT);

  // setup
  TIM5->PSC = 48-1; // tick on 1 us
  TIM5->CR1 = TIM_CR1_CEN;   // enable
  TIM5->ARR = 50-1;         // 50 us

  // in case it's disabled
  NVIC_EnableIRQ(TIM5_IRQn);

  // run the interrupt
  TIM5->DIER = TIM_DIER_UIE; // update interrupt
  TIM5->SR = 0;
}

void lline_relay_release (void) {
  set_gpio_alternate(GPIOC, 10, GPIO_AF7_USART3);
  NVIC_DisableIRQ(TIM5_IRQn);
}

void set_lline_output(int to_set) {
  timeout_cycles = LLINE_TIMEOUT_CYCLES;
  turn_on_relay = to_set;
}

int get_lline_status() {
  return turn_on_relay;
}

#endif
