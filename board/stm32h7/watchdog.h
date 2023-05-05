void watchdog_feed(void) {
  IWDG1->KR = 0xAAAAU;
}

void watchdog_init(void) {
  // setup watchdog
  IWDG1->KR = 0xCCCCU;
  IWDG1->KR = 0x5555U;

  // 32KHz clock

  // divider / 4
  IWDG1->SR &= ~(IWDG_SR_PVU_Msk);
  register_set(&(IWDG1->PR), 0x0U, IWDG_PR_PR_Msk);

  // 0 = 0.125 ms, let's have a 50ms watchdog
  IWDG1->SR &= ~(IWDG_SR_RVU_Msk);
  register_set(&(IWDG1->RLR), 4000U, IWDG_RLR_RL_Msk);

  // wait for registers to be updated
  while (IWDG1->SR != 0U);

  // start the watchdog
  //IWDG1->KR = 0xCCCCU;
  watchdog_feed();
}
