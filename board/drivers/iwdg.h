void watchdog_init(void) {
  // setup watchdog
  IWDG->KR = 0x5555U;
  register_set(&(IWDG->PR), 0x0U, 0x7U);  // divider/4

  // 0 = 0.125 ms, let's have a 50ms watchdog
  register_set(&(IWDG->RLR), (400U-1U), 0xFFFU);
  IWDG->KR = 0xCCCCU;
}

void watchdog_feed(void) {
  IWDG->KR = 0xAAAAU;
}
