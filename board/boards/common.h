#ifdef STM32H7
  #include "stm32h7xx_hal_gpio_ex.h"
#elif STM32F4
  #include "stm32f4xx_hal_gpio_ex.h"
#else
  #include "stm32f2xx_hal_gpio_ex.h"
#endif


// Common GPIO initialization
void common_init_gpio(void){
  
  #ifdef STM32H7
  // TODO: Is this block actually doing something???
    // pull low to hold ESP in reset??
    // enable OTG out tied to ground
    GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0;
    GPIOA->PUPDR = 0;
    GPIOB->AFR[0] = 0; GPIOB->AFR[1] = 0; GPIOC->AFR[0] = 0; GPIOC->AFR[1] = 0;

    // F14: Voltage sense line
    set_gpio_mode(GPIOF, 14, MODE_ANALOG);

    // A11,A12: USB
    set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG1_FS);
    set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG1_FS);
    GPIOA->OSPEEDR = GPIO_OSPEEDR_OSPEED11 | GPIO_OSPEEDR_OSPEED12;

    // B8, B9: FDCAN1
    set_gpio_alternate(GPIOB, 8, GPIO_AF9_FDCAN1);
    set_gpio_alternate(GPIOB, 9, GPIO_AF9_FDCAN1);
    // B5,B6: FDCAN2 (will be mplexed to B12,B13)
    set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);
    set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);
    // G9,G10: FDCAN3
    set_gpio_alternate(GPIOG, 9, GPIO_AF2_FDCAN3);
    set_gpio_alternate(GPIOG, 10, GPIO_AF2_FDCAN3);

    //MCO2 enable, divider is set to 15 in clock.h (To check system clock)
    //set_gpio_alternate(GPIOC, 9, GPIO_AF0_MCO);
    //MCO2 end
  #else
    // TODO: Is this block actually doing something???
    // pull low to hold ESP in reset??
    // enable OTG out tied to ground
    GPIOA->ODR = 0; GPIOB->ODR = 0;
    GPIOA->PUPDR = 0;
    GPIOB->AFR[0] = 0; GPIOB->AFR[1] = 0;

    // C2: Voltage sense line
    set_gpio_mode(GPIOC, 2, MODE_ANALOG);

    // A11,A12: USB
    set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG_FS);
    set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG_FS);
    GPIOA->OSPEEDR = GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12;

    // A9,A10: USART 1 for talking to the GPS
    set_gpio_alternate(GPIOA, 9, GPIO_AF7_USART1);
    set_gpio_alternate(GPIOA, 10, GPIO_AF7_USART1);

     // B8,B9: CAN 1
    #ifdef STM32F4
      set_gpio_alternate(GPIOB, 8, GPIO_AF8_CAN1);
      set_gpio_alternate(GPIOB, 9, GPIO_AF8_CAN1);
    #else
      set_gpio_alternate(GPIOB, 8, GPIO_AF9_CAN1);
      set_gpio_alternate(GPIOB, 9, GPIO_AF9_CAN1);
    #endif
  #endif
}


// Peripheral initialization
void peripherals_init(void){
  #ifdef STM32H7
  // enable GPIO(A,B,C,D,E,F,G,H), CANFD(1,2,3)
   RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
   RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
   RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN;
   RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
   RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
   RCC->AHB4ENR |= RCC_AHB4ENR_GPIOFEN;
   RCC->AHB4ENR |= RCC_AHB4ENR_GPIOGEN;
   RCC->AHB4ENR |= RCC_AHB4ENR_GPIOHEN;  //?

   // APB1 & APB2 - 275Mhz
   RCC->APB1HENR |= RCC_APB1HENR_FDCANEN;
   RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;  // main counter
   RCC->APB1LENR |= RCC_APB1LENR_TIM6EN;  // interrupt timer
   RCC->APB1LENR |= RCC_APB1LENR_TIM12EN;  // slow loop (moved to TIM12!!!)
   RCC->AHB1ENR |= RCC_AHB1ENR_USB1OTGHSEN; // HS USB enable
   ADC1->CR |= ADC_CR_ADEN; //enable ADC
   RCC->AHB4ENR |= RCC_APB4ENR_SYSCFGEN;
   RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;  // clock source timer
  #else
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
    //APB1 - 48Mhz, APB2 - 96Mhz ?
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // main counter
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;  // pedal and fan PWM
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;  // IR PWM
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;  // k-line init
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;  // interrupt timer
    RCC->APB1ENR |= RCC_APB1ENR_TIM12EN; // gmlan_alt
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;   // for RTC config
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;  // clock source timer
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM9EN;  // slow loop 
  #endif
}

// Detection with internal pullup
#define PULL_EFFECTIVE_DELAY 4096
bool detect_with_pull(GPIO_TypeDef *GPIO, int pin, int mode) {
  set_gpio_mode(GPIO, pin, MODE_INPUT);
  set_gpio_pullup(GPIO, pin, mode);
  for (volatile int i=0; i<PULL_EFFECTIVE_DELAY; i++);
  bool ret = get_gpio_input(GPIO, pin);
  set_gpio_pullup(GPIO, pin, PULL_NONE);
  return ret;
}
