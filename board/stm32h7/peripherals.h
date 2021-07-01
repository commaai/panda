// Common GPIO initialization
void common_init_gpio(void){
  // FIXME: as soon as development is over - clean up this section and turn off unneeded IOs with register_set
  /// E2,E3,E4: RGB LED
  set_gpio_speed(GPIOE, 2, SPEED_LOW); 
  set_gpio_output_type(GPIOE, 2, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOE, 2, PULL_NONE);
  set_gpio_mode(GPIOE, 2, MODE_OUTPUT);

  set_gpio_speed(GPIOE, 3, SPEED_LOW); 
  set_gpio_output_type(GPIOE, 3, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOE, 3, PULL_NONE);
  set_gpio_mode(GPIOE, 3, MODE_OUTPUT);

  set_gpio_speed(GPIOE, 4, SPEED_LOW);  
  set_gpio_output_type(GPIOE, 4, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOE, 4, PULL_NONE);
  set_gpio_mode(GPIOE, 4, MODE_OUTPUT);

  // F7,F8,F9,F10: BOARD ID
  set_gpio_pullup(GPIOF, 7, PULL_NONE);
  set_gpio_mode(GPIOF, 7, MODE_INPUT);

  set_gpio_pullup(GPIOF, 8, PULL_NONE);
  set_gpio_mode(GPIOF, 8, MODE_INPUT);

  set_gpio_pullup(GPIOF, 9, PULL_NONE);
  set_gpio_mode(GPIOF, 9, MODE_INPUT);

  set_gpio_pullup(GPIOF, 10, PULL_NONE);
  set_gpio_mode(GPIOF, 10, MODE_INPUT);

  // G11,B3,D7,B4: transceiver enable
  set_gpio_speed(GPIOG, 11, SPEED_LOW); 
  set_gpio_output_type(GPIOG, 11, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOG, 11, PULL_NONE);
  set_gpio_mode(GPIOG, 11, MODE_OUTPUT);

  set_gpio_speed(GPIOB, 3, SPEED_LOW); 
  set_gpio_output_type(GPIOB, 3, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 3, PULL_NONE);
  set_gpio_mode(GPIOB, 3, MODE_OUTPUT);

  set_gpio_speed(GPIOD, 7, SPEED_LOW); 
  set_gpio_output_type(GPIOD, 7, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOD, 7, PULL_NONE);
  set_gpio_mode(GPIOD, 7, MODE_OUTPUT);

  set_gpio_speed(GPIOB, 4, SPEED_LOW); 
  set_gpio_output_type(GPIOB, 4, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 4, PULL_NONE);
  set_gpio_mode(GPIOB, 4, MODE_OUTPUT);

  // B14: usb load switch
  set_gpio_speed(GPIOB, 14, SPEED_LOW); 
  set_gpio_output_type(GPIOB, 14, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 14, PULL_NONE);
  set_gpio_mode(GPIOB, 14, MODE_OUTPUT);

  //B1,F11 5VOUT_S, VOLT_S
  set_gpio_pullup(GPIOB, 1, PULL_NONE);
  set_gpio_mode(GPIOB, 1, MODE_ANALOG);

  set_gpio_pullup(GPIOF, 11, PULL_NONE);
  set_gpio_mode(GPIOF, 11, MODE_ANALOG);

  // A11,A12: USB:
  set_gpio_speed(GPIOA, 11, SPEED_VERY_HIGH); 
  set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG1_FS);

  set_gpio_speed(GPIOA, 12, SPEED_VERY_HIGH); 
  set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG1_FS);

  // B8,B9: FDCAN1
  set_gpio_output_type(GPIOB, 8, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 8, PULL_NONE);
  set_gpio_speed(GPIOB, 8, SPEED_LOW);
  set_gpio_alternate(GPIOB, 8, GPIO_AF9_FDCAN1);

  set_gpio_output_type(GPIOB, 9, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 9, PULL_NONE);
  set_gpio_speed(GPIOB, 9, SPEED_LOW);
  set_gpio_alternate(GPIOB, 9, GPIO_AF9_FDCAN1);
  
  // B5,B6 (mplex to B12,B13): FDCAN2
  set_gpio_output_type(GPIOB, 12, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 12, PULL_NONE);
  set_gpio_speed(GPIOB, 12, SPEED_LOW);
  set_gpio_output_type(GPIOB, 13, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 13, PULL_NONE);
  set_gpio_speed(GPIOB, 13, SPEED_LOW);

  set_gpio_output_type(GPIOB, 5, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 5, PULL_NONE);
  set_gpio_speed(GPIOB, 5, SPEED_LOW);
  set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);

  set_gpio_output_type(GPIOB, 6, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOB, 6, PULL_NONE);
  set_gpio_speed(GPIOB, 6, SPEED_LOW);
  set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);
  
  // G9,G10: FDCAN3
  set_gpio_output_type(GPIOG, 9, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOG, 9, PULL_NONE);
  set_gpio_speed(GPIOG, 9, SPEED_LOW);
  set_gpio_alternate(GPIOG, 9, GPIO_AF2_FDCAN3);

  set_gpio_output_type(GPIOG, 10, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_pullup(GPIOG, 10, PULL_NONE);
  set_gpio_speed(GPIOG, 10, SPEED_LOW);
  set_gpio_alternate(GPIOG, 10, GPIO_AF2_FDCAN3);

  //////////////////////////////////////
  // Unused pins to analog to save power
  // Port A
  set_gpio_pullup(GPIOA, 0, PULL_NONE);
  set_gpio_mode(GPIOA, 0, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 1, PULL_NONE);
  set_gpio_mode(GPIOA, 1, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 2, PULL_NONE);
  set_gpio_mode(GPIOA, 2, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 4, PULL_NONE);
  set_gpio_mode(GPIOA, 4, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 5, PULL_NONE);
  set_gpio_mode(GPIOA, 5, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 7, PULL_NONE);
  set_gpio_mode(GPIOA, 7, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 8, PULL_NONE);
  set_gpio_mode(GPIOA, 8, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 9, PULL_NONE);
  set_gpio_mode(GPIOA, 9, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 10, PULL_NONE);
  set_gpio_mode(GPIOA, 10, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 13, PULL_NONE);
  set_gpio_mode(GPIOA, 13, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 14, PULL_NONE);
  set_gpio_mode(GPIOA, 14, MODE_ANALOG);
  set_gpio_pullup(GPIOA, 15, PULL_NONE);
  set_gpio_mode(GPIOA, 15, MODE_ANALOG);
  // Port B
  set_gpio_pullup(GPIOB, 0, PULL_NONE);
  set_gpio_mode(GPIOB, 0, MODE_ANALOG);
  set_gpio_pullup(GPIOB, 2, PULL_NONE);
  set_gpio_mode(GPIOB, 2, MODE_ANALOG);
  set_gpio_pullup(GPIOB, 7, PULL_NONE);
  set_gpio_mode(GPIOB, 7, MODE_ANALOG);
  set_gpio_pullup(GPIOB, 10, PULL_NONE);
  set_gpio_mode(GPIOB, 10, MODE_ANALOG);
  set_gpio_pullup(GPIOB, 11, PULL_NONE);
  set_gpio_mode(GPIOB, 11, MODE_ANALOG);
  set_gpio_pullup(GPIOB, 12, PULL_NONE);
  set_gpio_mode(GPIOB, 12, MODE_ANALOG);
  set_gpio_pullup(GPIOB, 13, PULL_NONE);
  set_gpio_mode(GPIOB, 13, MODE_ANALOG);
  set_gpio_pullup(GPIOB, 15, PULL_NONE);
  set_gpio_mode(GPIOB, 15, MODE_ANALOG);
  // Port C
  set_gpio_pullup(GPIOC, 0, PULL_NONE);
  set_gpio_mode(GPIOC, 0, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 1, PULL_NONE);
  set_gpio_mode(GPIOC, 1, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 2, PULL_NONE);
  set_gpio_mode(GPIOC, 2, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 3, PULL_NONE);
  set_gpio_mode(GPIOC, 3, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 4, PULL_NONE);
  set_gpio_mode(GPIOC, 4, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 5, PULL_NONE);
  set_gpio_mode(GPIOC, 5, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 6, PULL_NONE);
  set_gpio_mode(GPIOC, 6, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 7, PULL_NONE);
  set_gpio_mode(GPIOC, 7, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 8, PULL_NONE);
  set_gpio_mode(GPIOC, 8, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 9, PULL_NONE);
  set_gpio_mode(GPIOC, 9, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 12, PULL_NONE); // OLD SBU1, moved to PA6
  set_gpio_mode(GPIOC, 12, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 13, PULL_NONE);
  set_gpio_mode(GPIOC, 13, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 14, PULL_NONE);
  set_gpio_mode(GPIOC, 14, MODE_ANALOG);
  set_gpio_pullup(GPIOC, 15, PULL_NONE);
  set_gpio_mode(GPIOC, 15, MODE_ANALOG);
  // Port D
  set_gpio_pullup(GPIOD, 0, PULL_NONE); // OLD SBU2, moved to PA3
  set_gpio_mode(GPIOD, 0, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 1, PULL_NONE);
  set_gpio_mode(GPIOD, 1, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 2, PULL_NONE);
  set_gpio_mode(GPIOD, 2, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 3, PULL_NONE);
  set_gpio_mode(GPIOD, 3, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 4, PULL_NONE);
  set_gpio_mode(GPIOD, 4, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 5, PULL_NONE);
  set_gpio_mode(GPIOD, 5, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 6, PULL_NONE);
  set_gpio_mode(GPIOD, 6, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 8, PULL_NONE);
  set_gpio_mode(GPIOD, 8, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 9, PULL_NONE);
  set_gpio_mode(GPIOD, 9, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 10, PULL_NONE);
  set_gpio_mode(GPIOD, 10, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 11, PULL_NONE);
  set_gpio_mode(GPIOD, 11, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 12, PULL_NONE);
  set_gpio_mode(GPIOD, 12, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 13, PULL_NONE);
  set_gpio_mode(GPIOD, 13, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 14, PULL_NONE);
  set_gpio_mode(GPIOD, 14, MODE_ANALOG);
  set_gpio_pullup(GPIOD, 15, PULL_NONE);
  set_gpio_mode(GPIOD, 15, MODE_ANALOG);
  // Port E
  set_gpio_pullup(GPIOE, 0, PULL_NONE);
  set_gpio_mode(GPIOE, 0, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 1, PULL_NONE);
  set_gpio_mode(GPIOE, 1, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 5, PULL_NONE);
  set_gpio_mode(GPIOE, 5, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 6, PULL_NONE);
  set_gpio_mode(GPIOE, 6, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 7, PULL_NONE);
  set_gpio_mode(GPIOE, 7, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 8, PULL_NONE);
  set_gpio_mode(GPIOE, 8, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 9, PULL_NONE);
  set_gpio_mode(GPIOE, 9, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 10, PULL_NONE);
  set_gpio_mode(GPIOE, 10, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 11, PULL_NONE);
  set_gpio_mode(GPIOE, 11, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 12, PULL_NONE);
  set_gpio_mode(GPIOE, 12, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 13, PULL_NONE);
  set_gpio_mode(GPIOE, 13, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 14, PULL_NONE);
  set_gpio_mode(GPIOE, 14, MODE_ANALOG);
  set_gpio_pullup(GPIOE, 15, PULL_NONE);
  set_gpio_mode(GPIOE, 15, MODE_ANALOG);
  // Port F
  set_gpio_pullup(GPIOF, 6, PULL_NONE);
  set_gpio_mode(GPIOF, 6, MODE_ANALOG);
  set_gpio_pullup(GPIOF, 14, PULL_NONE);
  set_gpio_mode(GPIOF, 14, MODE_ANALOG);
  set_gpio_pullup(GPIOF, 15, PULL_NONE);
  set_gpio_mode(GPIOF, 15, MODE_ANALOG);
  // Port G
  set_gpio_pullup(GPIOG, 6, PULL_NONE);
  set_gpio_mode(GPIOG, 6, MODE_ANALOG);
  set_gpio_pullup(GPIOG, 7, PULL_NONE);
  set_gpio_mode(GPIOG, 7, MODE_ANALOG);
  set_gpio_pullup(GPIOG, 8, PULL_NONE);
  set_gpio_mode(GPIOG, 8, MODE_ANALOG);
  set_gpio_pullup(GPIOG, 12, PULL_NONE);
  set_gpio_mode(GPIOG, 12, MODE_ANALOG);
  set_gpio_pullup(GPIOG, 13, PULL_NONE);
  set_gpio_mode(GPIOG, 13, MODE_ANALOG);
  set_gpio_pullup(GPIOG, 14, PULL_NONE);
  set_gpio_mode(GPIOG, 14, MODE_ANALOG);

  //C9: MCO2, divider is set in clock.h (To check system clock)
  //set_gpio_alternate(GPIOC, 9, GPIO_AF0_MCO);
}

// Peripheral initialization
void peripherals_init(void){
    // enable GPIO(A,B,C,D,E,F,G,H)
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN;
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOFEN;
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOGEN;

    RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;  // main counter
    RCC->APB1LENR |= RCC_APB1LENR_TIM6EN;  // interrupt timer
    RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;  // clock source timer (REDEBUG: move back to TIM1 ???)
    RCC->APB1LENR |= RCC_APB1LENR_TIM12EN;  // slow loop REDEBUG:(moved from TIM9 to TIM12!!!)

    //RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN; // Needed for USART2?
    //RCC->APB1LENR |= RCC_APB1LENR_USART2EN; // Enable USART2 for debug serial

    RCC->APB1HENR |= RCC_APB1HENR_FDCANEN; // CANFD(1,2,3) enable
    RCC->AHB1ENR |= RCC_AHB1ENR_USB1OTGHSEN; // HS USB enable
    RCC->AHB1ENR |= RCC_AHB1ENR_ADC12EN; // Enable ADC clocks
}
