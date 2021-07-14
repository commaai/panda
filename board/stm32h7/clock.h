void clock_init(void) {
  // Running at 240MHz

  //Set power mode to direct SMPS power supply(depends on the board layout)
  register_set(&(PWR->CR3), PWR_CR3_SMPSEN, 0xFU); // powered only by SMPS
  //register_set(&(PWR->CR3), PWR_CR3_LDOEN, 0x7U); // powered only by LDO

  //Set VOS level to VOS0. (VOS3 to 170Mhz, VOS2 to 300Mhz, VOS1 to 400Mhz, VOS0 to 550Mhz)
  //register_set(&(PWR->D3CR), 0x0U<<14U, 0xC000U); //VOS0
  //register_set(&(PWR->D3CR), PWR_D3CR_VOS_0 | PWR_D3CR_VOS_1, 0xC000U); //VOS1
  register_set(&(PWR->D3CR), PWR_D3CR_VOS_1, 0xC000U); //VOS2
  //register_set(&(PWR->D3CR), PWR_D3CR_VOS_0, 0xC000U); //VOS3
  while ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) == 0);
  while ((PWR->CSR1 & PWR_CSR1_ACTVOS) != (PWR->D3CR & PWR_D3CR_VOS)); // check that VOS level was actually set

  // Configure Flash ACR register LATENCY and WRHIGHFREQ (VOS0 range!)
  //register_set(&(FLASH->ACR), FLASH_ACR_LATENCY_3WS | 0x30U, 0x3FU); // VOS0, AXI 210MHz-275MHz
  //register_set(&(FLASH->ACR), FLASH_ACR_LATENCY_2WS | 0x20U, 0x3FU); // VOS1, AXI 133MHz-200MHz
  register_set(&(FLASH->ACR), FLASH_ACR_LATENCY_2WS | 0x20U, 0x3FU); // VOS2, AXI 100MHz-150MHz

  ////MCO2 DEBUG

  // Test clean HSE output
  //register_set(&(RCC->CFGR), RCC_CFGR_MCO2_1, RCC_CFGR_MCO2);

  // Test pll2_p output
  //register_set(&(RCC->CFGR), RCC_CFGR_MCO2_0, RCC_CFGR_MCO2);

  // Test with divider 4
  //register_set(&(RCC->CFGR), RCC_CFGR_MCO2PRE_2, RCC_CFGR_MCO2PRE);

  // Test with divider 15
  //register_set(&(RCC->CFGR), RCC_CFGR_MCO2PRE_0 | RCC_CFGR_MCO2PRE_1 | RCC_CFGR_MCO2PRE_2 | RCC_CFGR_MCO2PRE_3, RCC_CFGR_MCO2PRE);
  // DEBUG

  // enable external oscillator HSE
  register_set_bits(&(RCC->CR), RCC_CR_HSEON);
  while ((RCC->CR & RCC_CR_HSERDY) == 0);

  /// enable internal HSI48 (for USB)
  //register_set_bits(&(RCC->CR), RCC_CR_HSI48ON);
  //while ((RCC->CR & RCC_CR_HSI48RDY) == 0);

  // Specify the frequency source for PLL1, divider for DIVM1, divider for DIVM2 : HSE, 5, 5
  register_set(&(RCC->PLLCKSELR), RCC_PLLCKSELR_PLLSRC_HSE | RCC_PLLCKSELR_DIVM1_0 | RCC_PLLCKSELR_DIVM1_2 | RCC_PLLCKSELR_DIVM2_0 | RCC_PLLCKSELR_DIVM2_2, 0x3F3F3U);

  // *** PLL1 start ***
  // Specify multiplier N and dividers P, Q, R for PLL1 : 48, 1, 5, 2
  register_set(&(RCC->PLL1DIVR), 0x104002FU, 0x7F7FFFFFU);

  // Fractional frequency divider : 0
  //register_set(&(RCC->PLL1FRACR), 0x0U, 0xFFF8U);

  // Specify the input and output frequency ranges, enable dividers for PLL1
  register_set(&(RCC->PLLCFGR), RCC_PLLCFGR_PLL1RGE_2 | RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ1EN | RCC_PLLCFGR_DIVR1EN, 0x7000CU);

  // Enable PLL1
  register_set_bits(&(RCC->CR), RCC_CR_PLL1ON);
  while((RCC->CR & RCC_CR_PLL1RDY) == 0);
  // *** PLL1 end ***

  // *** PLL2 start ***
  // Specify multiplier N and dividers P, Q, R for PLL2 : 48, 4, 2, 2
  //register_set(&(RCC->PLL2DIVR), 0x101062FU, 0x7F7FFFFFU);

  // Fractional frequency divider : 0
  //register_set(&(RCC->PLL2FRACR), 0x0U, 0xFFF8U);

  // Specify the input and output frequency ranges, enable dividers for PLL2: 8 to 16Mhz, wide VCO range(0)
  //register_set(&(RCC->PLLCFGR), RCC_PLLCFGR_PLL2RGE_2 | ~RCC_PLLCFGR_PLL2VCOSEL | RCC_PLLCFGR_DIVP2EN | RCC_PLLCFGR_DIVQ2EN | RCC_PLLCFGR_DIVR2EN, 0x3800E0U); //RCC_PLLCFGR_PLL2FRACEN

  // Fractional divider must be also enabled if being used!

  // Enable PLL2
  //register_set_bits(&(RCC->CR), RCC_CR_PLL2ON);
  //while((RCC->CR & RCC_CR_PLL2RDY) == 0);
  // *** PLL2 end ***

  //////////////OTHER CLOCKS////////////////////
  // RCC HCLK Clock Source / RCC APB3 Clock Source / RCC SYS Clock Source
  register_set(&(RCC->D1CFGR), RCC_D1CFGR_HPRE_DIV2 | RCC_D1CFGR_D1PPRE_DIV2 | RCC_D1CFGR_D1CPRE_DIV1, 0xF7FU);

  // RCC APB1 Clock Source / RCC APB2 Clock Source
  register_set(&(RCC->D2CFGR), RCC_D2CFGR_D2PPRE1_DIV2 | RCC_D2CFGR_D2PPRE2_DIV2, 0x770U);

  // RCC APB4 Clock Source
  register_set(&(RCC->D3CFGR), RCC_D3CFGR_D3PPRE_DIV2, 0x70U);

  // PLL2P for ADC
  register_set(&(RCC->D3CFGR), 0x0U, RCC_D3CCIPR_ADCSEL);

  // Set SysClock source to PLL
  register_set(&(RCC->CFGR), RCC_CFGR_SW_PLL1, 0x7U);
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL1);

  // SYSCFG peripheral clock enable
  register_set_bits(&(RCC->AHB4ENR), RCC_APB4ENR_SYSCFGEN);
  //////////////END OTHER CLOCKS////////////////////

  // Configure clock source for USB
  //register_set(&(RCC->D2CCIP2R), RCC_D2CCIP2R_USBSEL_0 | RCC_D2CCIP2R_USBSEL_1, RCC_D2CCIP2R_USBSEL); //HSI48
  register_set(&(RCC->D2CCIP2R), RCC_D2CCIP2R_USBSEL_0, RCC_D2CCIP2R_USBSEL); //PLL1Q

  // Configure clock source for FDCAN
  register_set(&(RCC->D2CCIP1R), RCC_D2CCIP1R_FDCANSEL_0, RCC_D2CCIP1R_FDCANSEL); //PLL1Q

  // Configure clock source for ADC1,2,3
  register_set(&(RCC->D3CCIPR), RCC_D3CCIPR_ADCSEL_1, RCC_D3CCIPR_ADCSEL); //per_ck(currently HSE)

  // *** End config of high performance mode at 550Mhz ***

  //Enable the Clock Security System
  register_set_bits(&(RCC->CR), RCC_CR_CSSHSEON);

  //Enable Vdd33usb supply level detector
  register_set_bits(&(PWR->CR3), PWR_CR3_USB33DEN);
}
