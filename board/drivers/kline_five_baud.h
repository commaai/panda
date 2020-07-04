#define KLINE_5BAUD_TICKS_PER_BIT 40 // 200ms @ 5bps

void TIM5_IRQ_Handler(void);

void setup_timer5(void) {
  // register interrupt
  REGISTER_INTERRUPT(TIM5_IRQn, TIM5_IRQ_Handler, 1000000U, 1000000)

  // setup
  register_set(&(TIM5->PSC), (48-1), 0xFFFFU);    // Tick on 1 us
  register_set(&(TIM5->CR1), TIM_CR1_CEN, 0x3FU); // Enable
  register_set(&(TIM5->ARR), (5000-1), 0xFFFFU);  // Reset every 5 ms

  // in case it's disabled
  NVIC_EnableIRQ(TIM5_IRQn);

  // run the interrupt
  register_set(&(TIM5->DIER), TIM_DIER_UIE, 0x5F5FU); // Update interrupt
  TIM5->SR = 0;
  puts("timer setup done\n");
}

bool k_init = false;
bool l_init = false;
void setup_kline(bool bitbang) {
  if (bitbang) {
    if (k_init) {
      set_gpio_output(GPIOC, 12, true);
    }
    if (l_init) {
      set_gpio_output(GPIOC, 10, true);
    }
  } else {
    if (k_init) {
      set_gpio_mode(GPIOC, 12, MODE_ALTERNATE);
    }
    if (k_init) {
      set_gpio_mode(GPIOC, 10, MODE_ALTERNATE);
    }
  }
  puts("kline setup done\n");
}

void set_bitbanged_kline(bool marking) {
  if (k_init) {
    register_set_bits(&(GPIOC->ODR), (1U << 12));
  }
  if (l_init) {
    register_set_bits(&(GPIOC->ODR), (1U << 10));
  }
  if (!marking) {
    if (k_init) {
      register_clear_bits(&(GPIOC->ODR), (1U << 12));
    }
    if (l_init) {
      register_clear_bits(&(GPIOC->ODR), (1U << 10));
    }
  }
  // blink blue LED each time line is pulled low
  current_board->set_led(LED_BLUE, !marking);
}

uint16_t kline_five_baud_dat = 0;
int kline_five_baud_bit = -1;
int kline_tick_count = 0;

void TIM5_IRQ_Handler(void) {
  puts("timer fired\n");
  if ((TIM5->SR & TIM_SR_UIF) && (kline_five_baud_bit != -1)) {
    if ((kline_tick_count % KLINE_5BAUD_TICKS_PER_BIT) == 0) {
      kline_five_baud_bit++;
    }
    if (kline_five_baud_bit > 10) {
      register_clear_bits(&(TIM5->DIER), TIM_DIER_UIE); // No update interrupt
      register_set(&(TIM5->CR1), 0U, 0x3FU); // Disable timer
      setup_kline(false);
      kline_five_baud_bit = -1;
    } else {
      bool marking = (kline_five_baud_dat & (1U << kline_five_baud_bit)) != 0U;
      set_bitbanged_kline(marking);
    }
    kline_tick_count++;
  }
  TIM5->SR = 0;
}

void bitbang_five_baud_addr(bool k, bool l, uint8_t addr) {
  if (kline_five_baud_bit == -1) {
    k_init = k;
    l_init = l;
    kline_five_baud_dat = (addr << 1); // add start bit
    kline_tick_count = 0;
    kline_five_baud_bit = 0;
    setup_kline(true);
    setup_timer5();
  }
}
