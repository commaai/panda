// TODO: why doesn't it define these?
#ifdef STM32F2
#define IWDG_PR_PR_Msk 0x7U
#define IWDG_RLR_RL_Msk 0xFFFU
#endif

void watchdog_feed(void) {
  IND_WDG->KR = 0xAAAAU;
}

void watchdog_init(void) {
  // setup watchdog
  IND_WDG->KR = 0xCCCCU;
  IND_WDG->KR = 0x5555U;

  // 32KHz clock

  // divider / 4
  register_set(&(IND_WDG->PR), 0x0U, IWDG_PR_PR_Msk);

  // 0 = 0.125 ms, let's have a 50ms watchdog
  register_set(&(IND_WDG->RLR), 4000U, IWDG_RLR_RL_Msk);

  // wait for registers to be updated
  while (IND_WDG->SR != 0U);

  // start the watchdog
  //IND_WDG->KR = 0xCCCCU;
  watchdog_feed();
}
