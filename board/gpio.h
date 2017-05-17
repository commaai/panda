#ifdef STM32F4
  #include "stm32f4xx_hal_gpio_ex.h"
#else
  #include "stm32f2xx_hal_gpio_ex.h"
#endif

void periph_init() { 
  // enable GPIOB, UART2, CAN, USB clock
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
  #ifdef PANDA
    RCC->APB1ENR |= RCC_APB1ENR_UART5EN;
  #endif
  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
  #ifdef CAN3
    RCC->APB1ENR |= RCC_APB1ENR_CAN3EN;
  #endif
  RCC->APB1ENR |= RCC_APB1ENR_DACEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  //RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

  // needed?
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
}

void set_can2_mode(int use_gmlan) {

  // http://www.bittiming.can-wiki.info/#bxCAN
  // 24 MHz, sample point at 87.5%
  uint32_t pclk = 24000;
  uint32_t num_time_quanta = 16;
  uint32_t prescaler;

  // connects to CAN2 xcvr or GMLAN xcvr
  if (use_gmlan) {
    // disable normal mode
    GPIOB->MODER &= ~(GPIO_MODER_MODER5_1 | GPIO_MODER_MODER6_1);
    GPIOB->AFR[0] &= ~(GPIO_AF9_CAN2 << (5*4) | GPIO_AF9_CAN2 << (6*4));

    // gmlan mode
    GPIOB->MODER |= GPIO_MODER_MODER12_1 | GPIO_MODER_MODER13_1;
    GPIOB->AFR[1] |= GPIO_AF9_CAN2 << ((12-8)*4) | GPIO_AF9_CAN2 << ((13-8)*4);

    /* GMLAN mode pins:
    M0(B15)  M1(B14)  mode
    =======================
    0        0        sleep
    1        0        100kbit
    0        1        high voltage wakeup
    1        1        33kbit (normal)
    */
    GPIOB->ODR |= (1 << 15) | (1 << 14);
    GPIOB->MODER |= GPIO_MODER_MODER14_0 | GPIO_MODER_MODER15_0;

    // 83.3 kbps
    // prescaler = pclk / num_time_quanta * 10 / 833;

    // 33.3 kbps
    prescaler = pclk / num_time_quanta * 10 / 333;
  } else {
    // disable GMLAN
    GPIOB->MODER &= ~(GPIO_MODER_MODER12_1 | GPIO_MODER_MODER13_1);
    GPIOB->AFR[1] &= ~(GPIO_AF9_CAN2 << ((12-8)*4) | GPIO_AF9_CAN2 << ((13-8)*4));

    // normal mode
    GPIOB->MODER |= GPIO_MODER_MODER5_1 | GPIO_MODER_MODER6_1;
    GPIOB->AFR[0] |= GPIO_AF9_CAN2 << (5*4) | GPIO_AF9_CAN2 << (6*4);

    // 500 kbps
    prescaler = pclk / num_time_quanta / 500;
  }

  // init
  CAN2->MCR = CAN_MCR_TTCM | CAN_MCR_INRQ;
  while((CAN2->MSR & CAN_MSR_INAK) != CAN_MSR_INAK);

  // set speed
  // seg 1: 13 time quanta, seg 2: 2 time quanta
  CAN2->BTR = (CAN_BTR_TS1_0 * 12) |
    CAN_BTR_TS2_0 | (prescaler - 1);

  // running
  CAN2->MCR = CAN_MCR_TTCM;
  while((CAN2->MSR & CAN_MSR_INAK) == CAN_MSR_INAK);
}

// board specific
void gpio_init() {
  // pull low to hold ESP in reset??
  // enable OTG out tied to ground
  GPIOA->ODR = 0;
  GPIOB->ODR = 0;
  GPIOA->PUPDR = 0;
  //GPIOC->ODR = 0;
  GPIOB->AFR[0] = 0;
  GPIOB->AFR[1] = 0;

  // enable USB power tied to +
  GPIOA->MODER = GPIO_MODER_MODER0_0;

  // always set to zero, ESP in boot mode and reset
  //GPIOB->MODER = GPIO_MODER_MODER0_0;

  // analog mode
  GPIOC->MODER |= GPIO_MODER_MODER3 | GPIO_MODER_MODER2;
  //GPIOC->MODER |= GPIO_MODER_MODER1 | GPIO_MODER_MODER0;

  // FAN on C9, aka TIM3_CH4
  GPIOC->MODER |= GPIO_MODER_MODER8_1;
  GPIOC->AFR[1] = GPIO_AF2_TIM3 << ((8-8)*4);
  // IGNITION on C13

  #ifdef PANDA
    // turn off LEDs and set mode
    GPIOC->ODR |= (1 << 6) | (1 << 7) | (1 << 9);
    GPIOC->MODER |= GPIO_MODER_MODER6_0 | GPIO_MODER_MODER7_0 | GPIO_MODER_MODER9_0;

    // set mode for ESP_RST line and enable it
    // (done in init_hardware_early
    //GPIOC->ODR |= (1 << 14) | (1 << 5);
    //GPIOC->MODER |= GPIO_MODER_MODER14_0 | GPIO_MODER_MODER5_0;

    // panda CAN enables
    GPIOC->ODR |= (1 << 13) | (1 << 1);
    GPIOC->MODER |= GPIO_MODER_MODER13_0 | GPIO_MODER_MODER1_0;

    // enable started_alt
    GPIOA->PUPDR |= GPIO_PUPDR_PUPDR1_0;
  #else
    // turn off LEDs and set mode
    GPIOB->ODR = (1 << 10) | (1 << 11) | (1 << 12);
    GPIOB->MODER = GPIO_MODER_MODER10_0 | GPIO_MODER_MODER11_0 | GPIO_MODER_MODER12_0;
    
    // non panda CAN enables
    GPIOB->MODER |= GPIO_MODER_MODER3_0 | GPIO_MODER_MODER7_0;
  #endif

  // CAN 2 in normal mode
  set_can2_mode(0);

  // CAN 1
  GPIOB->MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1;
#ifdef STM32F4
  GPIOB->AFR[1] |= GPIO_AF8_CAN1 << ((8-8)*4) | GPIO_AF8_CAN1 << ((9-8)*4);
#else
  GPIOB->AFR[1] |= GPIO_AF9_CAN1 << ((8-8)*4) | GPIO_AF9_CAN1 << ((9-8)*4);
#endif

  // K enable + L enable
#ifdef REVC
  // K-line enable moved from B4->B7 to make room for GMLAN on CAN3
  GPIOB->ODR |= (1 << 7);
  GPIOB->MODER |= GPIO_MODER_MODER7_0;
#else
  GPIOB->ODR |= (1 << 4);
  GPIOB->MODER |= GPIO_MODER_MODER4_0;
#endif

  GPIOA->ODR |= (1 << 14);
  GPIOA->MODER |= GPIO_MODER_MODER14_0;

#ifdef REVC
  // set DCP mode on the charger
  /*GPIOB->ODR &= ~(1 << 2);
  GPIOB->MODER |= GPIO_MODER_MODER2_0;
  GPIOA->ODR &= ~(1 << 13);
  GPIOA->MODER |= GPIO_MODER_MODER13_0;*/
#endif

  // USART 2 for debugging
  GPIOA->MODER |= GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;
  GPIOA->AFR[0] = GPIO_AF7_USART2 << (2*4) | GPIO_AF7_USART2 << (3*4);

  // USART 1 for talking to the ESP
  GPIOA->MODER |= GPIO_MODER_MODER9_1 | GPIO_MODER_MODER10_1;
  GPIOA->AFR[1] = GPIO_AF7_USART1 << ((9-8)*4) | GPIO_AF7_USART1 << ((10-8)*4);

  // USART 3 is L-Line
  GPIOC->MODER |= GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1;
  GPIOC->AFR[1] |= GPIO_AF7_USART3 << ((10-8)*4) | GPIO_AF7_USART3 << ((11-8)*4);
  GPIOC->PUPDR = GPIO_PUPDR_PUPDR11_0;

  // USB
  GPIOA->MODER |= GPIO_MODER_MODER11_1 | GPIO_MODER_MODER12_1;
  GPIOA->OSPEEDR = GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12;
  GPIOA->AFR[1] |= GPIO_AF10_OTG_FS << ((11-8)*4) | GPIO_AF10_OTG_FS << ((12-8)*4);
  GPIOA->PUPDR |= GPIO_PUPDR_PUPDR2_0 | GPIO_PUPDR_PUPDR3_0;

  // GMLAN, ignition sense, pull up
  GPIOB->PUPDR |= GPIO_PUPDR_PUPDR12_0;

  // setup SPI
  GPIOA->MODER |= GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1 |
                  GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1;
  GPIOA->AFR[0] |= GPIO_AF5_SPI1 << (4*4) | GPIO_AF5_SPI1 << (5*4) |
                   GPIO_AF5_SPI1 << (6*4) | GPIO_AF5_SPI1 << (7*4);

  // CAN3 setup
  #ifdef CAN3
    GPIOA->MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER15_1;
    GPIOA->AFR[1] |= GPIO_AF11_CAN3 << ((8-8)*4) | GPIO_AF11_CAN3 << ((15-8)*4);
  #endif

  // K-Line setup
  #ifdef PANDA
    GPIOC->AFR[1] |= GPIO_AF8_UART5 << ((12-8)*4);
    GPIOC->MODER |= GPIO_MODER_MODER12_1;
    GPIOD->AFR[0] = GPIO_AF8_UART5 << (2*4);
    GPIOD->MODER = GPIO_MODER_MODER2_1;
    GPIOD->PUPDR = GPIO_PUPDR_PUPDR2_0;
  #endif
}

#ifdef PANDA
  #define LED_RED 3
  #define LED_GREEN 1
  #define LED_BLUE 0
#else
  #define LED_RED 0
  #define LED_GREEN 1
  #define LED_BLUE -1
#endif

void set_led(int led_num, int on) {
  if (led_num == -1) return;
  if (on) {
    // turn on
    #ifdef PANDA
      GPIOC->ODR &= ~(1 << (6 + led_num));
    #else
      GPIOB->ODR &= ~(1 << (10 + led_num));
    #endif
  } else {
    // turn off
    #ifdef PANDA
      GPIOC->ODR |= (1 << (6 + led_num));
    #else
      GPIOB->ODR |= (1 << (10 + led_num));
    #endif
  }
}


