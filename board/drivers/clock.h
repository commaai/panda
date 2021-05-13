void clock_init(void) {
  // enable external oscillator
  register_set_bits(&(RCC->CR), RCC_CR_HSEON);
  while ((RCC->CR & RCC_CR_HSERDY) == 0);

  // divide things
  register_set(&(RCC->CFGR), RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_PPRE1_DIV4, 0xFF7FFCF3U);

  // 16mhz crystal
  register_set(&(RCC->PLLCFGR), RCC_PLLCFGR_PLLQ_2 | RCC_PLLCFGR_PLLM_3 | RCC_PLLCFGR_PLLN_6 | RCC_PLLCFGR_PLLN_5 | RCC_PLLCFGR_PLLSRC_HSE, 0x7F437FFFU);

  // start PLL
  register_set_bits(&(RCC->CR), RCC_CR_PLLON);
  while ((RCC->CR & RCC_CR_PLLRDY) == 0);

  // Configure Flash prefetch, Instruction cache, Data cache and wait state
  // *** without this, it breaks ***
  register_set(&(FLASH->ACR), FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS, 0x1F0FU);

  // switch to PLL
  register_set_bits(&(RCC->CFGR), RCC_CFGR_SW_PLL);
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

  // *** running on PLL ***
}

// Running clock at 550Mhz
void clock_init_h7(void) {
  //Set power mode to direct SMPS power supply
  register_set(&(PWR->CR3), PWR_CR3_SMPSEN, 0x3FU);

  // enable external oscillator HSE
  register_set_bits(&(RCC->CR), RCC_CR_HSEON);
  while ((RCC->CR & RCC_CR_HSERDY) == 0);

  // Specify the frequency source for PLL1, divider for DIVM1, divider for DIVM2 : HSE, 2, 2
  register_set(&(RCC->PLLCKSELR), RCC_PLLCKSELR_PLLSRC_HSE | RCC_PLLCKSELR_DIVM1_1 | RCC_PLLCKSELR_DIVM2_1, 0x3F3F3U); //Full reg mask: 0x3F3F3F3U

  // *** PLL1 start ***
  // Specify multiplier N and dividers P, Q, R for PLL1 : 44, 1, 5, 2
  register_set(&(RCC->PLL1DIVR), 0x104002BU, 0x7F7FFFFFU);

  // Fractional frequency divider : 0
  register_set(&(RCC->PLL1FRACR), 0x0U, 0xFFF8U);

  // Specify the input and output frequency ranges, enable dividers for PLL1: 8 to 16Mhz, wide VCO range(0)
  register_set(&(RCC->PLLCFGR), RCC_PLLCFGR_PLL1RGE_3 | ~RCC_PLLCFGR_PLL1VCOSEL | RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ1EN | RCC_PLLCFGR_DIVR1EN | RCC_PLLCFGR_PLL1FRACEN, 0x7000FU); //Full reg mask: 0x1FF0FFFU

  // Enable PLL1
  register_set_bits(&(RCC->CR), RCC_CR_PLL1ON);
  while((RCC->CR & RCC_CR_PLL1RDY) == 0);
  // *** PLL1 end ***

  // *** PLL2 start (used for ADC)***
  // Specify multiplier N and dividers P, Q, R for PLL2 : 15, 2, 4, 2
  register_set(&(RCC->PLL2DIVR), 0x103020EU, 0x7F7FFFFFU);

  // Fractional frequency divider : 2950
  register_set(&(RCC->PLL2FRACR), 0x5C30U, 0xFFF8U);

  // Specify the input and output frequency ranges, enable dividers for PLL2: 8 to 16Mhz, wide VCO range(0)
  register_set(&(RCC->PLLCFGR), RCC_PLLCFGR_PLL2RGE_3 | ~RCC_PLLCFGR_PLL2VCOSEL | RCC_PLLCFGR_DIVP2EN | RCC_PLLCFGR_DIVQ2EN | RCC_PLLCFGR_DIVR2EN | RCC_PLLCFGR_PLL2FRACEN, 0x3800F0U); //Full reg mask: 0x1FF0FFFU

  // Select PLL2 for ADC
  register_set(&(RCC->D3CCIPR), 0x0U ,0x30000U);

  // Enable PLL2
  register_set_bits(&(RCC->CR), RCC_CR_PLL2ON);
  while((RCC->CR & RCC_CR_PLL2RDY) == 0);

  // *** PLL2 end ***

  // divide things for Domain 1
  register_set(&(RCC->D1CFGR), RCC_D1CFGR_HPRE_DIV2 | RCC_D1CFGR_D1PPRE_DIV2 | RCC_D1CFGR_D1CPRE_DIV1, 0xF7FU);

  // Set SysClock source to PLL
  register_set(&(RCC->CFGR), RCC_CFGR_SW_PLL1, 0x7U); //Full reg mask: 0xFFFCBFC7U
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL1);

  // Divider by 2 for D2PPRE1 and D2PPRE2
  register_set(&(RCC->D2CFGR), RCC_D2CFGR_D2PPRE1_DIV2 | RCC_D2CFGR_D2PPRE2_DIV2, 0x770U);

  // Divider by 2 for D3PPRE
  register_set(&(RCC->D3CFGR), RCC_D3CFGR_D3PPRE_DIV2, 0x70U);

  // Configure Flash ACR register LATENCY and WRHIGHFREQ
  register_set(&(FLASH->ACR), FLASH_ACR_LATENCY_3WS | 0x30U, 0x3FU);

  //Enable the Clock Security System
  register_set_bits(&(RCC->CR), RCC_CR_CSSHSEON);

  //Enable Vdd33usb supply level detector
  register_set_bits(&(PWR->CR3), PWR_CR3_USB33DEN);
}

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

