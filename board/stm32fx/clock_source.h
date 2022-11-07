#define CLOCK_SOURCE_MODE_DISABLED       0U
#define CLOCK_SOURCE_MODE_PWM            2U

uint8_t clock_source_mode = CLOCK_SOURCE_MODE_DISABLED;

void clock_source_init(uint8_t mode){
  // Set mode
  switch(mode) {
    case CLOCK_SOURCE_MODE_DISABLED:
      // No clock signal
      NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);
      NVIC_DisableIRQ(TIM1_CC_IRQn);

      // Disable pulse if we were in the middle of it
      set_gpio_output(GPIOB, 14, false);
      set_gpio_output(GPIOB, 15, false);

      clock_source_mode = CLOCK_SOURCE_MODE_DISABLED;
      break;
    case CLOCK_SOURCE_MODE_PWM:
      // No interrupts
      NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);
      NVIC_DisableIRQ(TIM1_CC_IRQn);

      // Set GPIO as timer channels
      set_gpio_alternate(GPIOB, 14, GPIO_AF1_TIM1);
      set_gpio_alternate(GPIOB, 15, GPIO_AF1_TIM1);

      // Set PWM mode
      register_set(&(TIM1->CCMR1), (0b110 << TIM_CCMR1_OC2M_Pos), 0xFFFFU);
      register_set(&(TIM1->CCMR2), (0b110 << TIM_CCMR2_OC3M_Pos), 0xFFFFU);

      // Enable output
      register_set(&(TIM1->BDTR), TIM_BDTR_MOE, 0xFFFFU);

      // Enable complementary compares
      register_set_bits(&(TIM1->CCER), TIM_CCER_CC2NE | TIM_CCER_CC3NE);

      clock_source_mode = CLOCK_SOURCE_MODE_PWM;
      break;
    default:
      puts("Unknown clock source mode: "); puth(mode); puts("\n");
      break;
  }
}
