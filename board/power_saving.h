void power_save_enable(void) {
  puts("Saving power\n");
  //Turn off can transciever
  set_can_enable(CAN1, 0);
  set_can_enable(CAN2, 0);
  #ifdef PANDA
    set_can_enable(CAN3, 0);
  #endif

  //Turn off GMLAN
  set_gpio_output(GPIOB, 14, 0);
  set_gpio_output(GPIOB, 15, 0);

  #ifdef PANDA
    //Turn off LIN K
    if (revision == PANDA_REV_C) {
      set_gpio_output(GPIOB, 7, 0); // REV C
    } else {
      set_gpio_output(GPIOB, 4, 0); // REV AB
    }
    // LIN L
    set_gpio_output(GPIOA, 14, 0);
  #endif

  if (is_grey_panda) {
    char* UBLOX_SLEEP_MSG = "\xb5\x62\x06\x04\x04\x00\x01\x00\x08\x00\x17\x78";
    int len = 12;
    uart_ring *ur = get_ring_by_number(1);
    for (int i = 0; i < len; i++) while (!putc(ur, UBLOX_SLEEP_MSG[i]));
  }
}

void power_save_disable(void) {
  puts("not Saving power\n");
  TIM6->CR1 |= TIM_CR1_CEN; //Restart timer
  TIM6->CNT = 0;

  //Turn on can
  set_can_enable(CAN1, 1);
  set_can_enable(CAN2, 1);
  #ifdef PANDA
    set_can_enable(CAN3, 1);
  #endif

  //Turn on GMLAN
  set_gpio_output(GPIOB, 14, 1);
  set_gpio_output(GPIOB, 15, 1);

  #ifdef PANDA
    //Turn on LIN K
    if (revision == PANDA_REV_C) {
      set_gpio_output(GPIOB, 7, 1); // REV C
    } else {
      set_gpio_output(GPIOB, 4, 1); // REV AB
    }
    // LIN L
    set_gpio_output(GPIOA, 14, 1);
  #endif

  if (is_grey_panda) {
    char* UBLOX_WAKE_MSG = "\xb5\x62\x06\x04\x04\x00\x01\x00\x09\x00\x18\x7a";
    int len = 12;
    uart_ring *ur = get_ring_by_number(1);
    for (int i = 0; i < len; i++) while (!putc(ur, UBLOX_WAKE_MSG[i]));
  }
}

// Reset timer when activity
void power_save_reset_timer() {
  TIM6->CNT = 0;
  if (!(TIM6->CR1 & TIM_CR1_CEN)){
#ifdef EON
    power_save_disable();
#endif
  }
}

void power_save_init(void) {
  puts("Saving power init\n");
  TIM6->PSC = 48000-1; // tick on 1 ms


  TIM6->ARR = 10000; // 10s
  // Enable, One-Pulse Mode, Only overflow interrupt
  TIM6->CR1 = TIM_CR1_CEN | TIM_CR1_OPM | TIM_CR1_URS;
  TIM6->EGR = TIM_EGR_UG;
  NVIC_EnableIRQ(TIM6_DAC_IRQn);
  puts("Saving power init done\n");
  TIM6->DIER = TIM_DIER_UIE;
  TIM6->CR1 |= TIM_CR1_CEN;
}

void TIM6_DAC_IRQHandler(void) {
  //Timeout switch to power saving mode.
  puts("TIM6\n");
  if (TIM6->SR & TIM_SR_UIF) {
    TIM6->SR = 0;
#ifdef EON
    power_save_enable();
#endif
  } else {
    TIM6->CR1 |= TIM_CR1_CEN;
  }
}
