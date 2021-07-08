#define RCC_BDCR_OPTIONS (RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_0 | RCC_BDCR_LSEON)
#define RCC_BDCR_MASK (RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL | RCC_BDCR_LSEMOD | RCC_BDCR_LSEBYP | RCC_BDCR_LSEON)

void rtc_init(void){
  if(current_board->has_rtc){
    // Initialize RTC module and clock if not done already.
    if((RCC->BDCR & RCC_BDCR_MASK) != RCC_BDCR_OPTIONS){
      puts("Initializing RTC\n");
      // Reset backup domain
      register_set_bits(&(RCC->BDCR), RCC_BDCR_BDRST);

      // Disable write protection
      register_set_bits(&(PWR->CR), PWR_CR_DBP);

      // Clear backup domain reset
      register_clear_bits(&(RCC->BDCR), RCC_BDCR_BDRST);

      // Set RTC options
      register_set(&(RCC->BDCR), RCC_BDCR_OPTIONS, RCC_BDCR_MASK);

      // Enable write protection
      register_clear_bits(&(PWR->CR), PWR_CR_DBP);
    }
  }
}

void rtc_set_time(timestamp_t time){
  if(current_board->has_rtc){
    puts("Setting RTC time\n");

    // Disable write protection
    register_set_bits(&(PWR->CR), PWR_CR_DBP);
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;

    // Enable initialization mode
    register_set_bits(&(RTC->ISR), RTC_ISR_INIT);
    while((RTC->ISR & RTC_ISR_INITF) == 0);

    // Set time
    RTC->TR = (to_bcd(time.hour) << RTC_TR_HU_Pos) | (to_bcd(time.minute) << RTC_TR_MNU_Pos) | (to_bcd(time.second) << RTC_TR_SU_Pos);
    RTC->DR = (to_bcd(time.year - YEAR_OFFSET) << RTC_DR_YU_Pos) | (time.weekday << RTC_DR_WDU_Pos) | (to_bcd(time.month) << RTC_DR_MU_Pos) | (to_bcd(time.day) << RTC_DR_DU_Pos);

    // Set options
    register_set(&(RTC->CR), 0U, 0xFCFFFFU);

    // Disable initalization mode
    register_clear_bits(&(RTC->ISR), RTC_ISR_INIT);

    // Wait for synchronization
    while((RTC->ISR & RTC_ISR_RSF) == 0);

    // Re-enable write protection
    RTC->WPR = 0x00;
    register_clear_bits(&(PWR->CR), PWR_CR_DBP);
  }
}
