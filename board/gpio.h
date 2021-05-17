// Early bringup
#define ENTER_BOOTLOADER_MAGIC 0xdeadbeef
#define ENTER_SOFTLOADER_MAGIC 0xdeadc0de
#define BOOT_NORMAL 0xdeadb111

extern void *g_pfnVectors;
extern uint32_t enter_bootloader_mode;

void jump_to_bootloader(void) {
  // do enter bootloader
  enter_bootloader_mode = 0;
  #ifdef STM32H7
    void (*bootloader)(void) = (void (*)(void)) (*((uint32_t *)0x1FF09804)); // Fix this for H7? 84Kb starting from 0x1FFF9800 
  #else
    void (*bootloader)(void) = (void (*)(void)) (*((uint32_t *)0x1fff0004));
  #endif
  // jump to bootloader
  enable_interrupts();
  bootloader();

  // reset on exit
  enter_bootloader_mode = BOOT_NORMAL;
  NVIC_SystemReset();
}

void early(void) {
  // Reset global critical depth
  disable_interrupts();
  global_critical_depth = 0;

  // Init register and interrupt tables
  init_registers();

  // after it's been in the bootloader, things are initted differently, so we reset
  if ((enter_bootloader_mode != BOOT_NORMAL) &&
      (enter_bootloader_mode != ENTER_BOOTLOADER_MAGIC) &&
      (enter_bootloader_mode != ENTER_SOFTLOADER_MAGIC)) {
    enter_bootloader_mode = BOOT_NORMAL;
    NVIC_SystemReset();
  }

  // if wrong chip, reboot
  volatile unsigned int id = DBGMCU->IDCODE;
  #ifdef STM32H7
    if ((id & 0xFFFU) != 0x483U) {
      enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
    }
  #elif STM32F4
    if ((id & 0xFFFU) != 0x463U) {
      enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
    }
  #else
    if ((id & 0xFFFU) != 0x411U) {
      enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
    }
  #endif

  // setup interrupt table
  SCB->VTOR = (uint32_t)&g_pfnVectors;

  // early GPIOs float everything
  #ifdef STM32H7
    RCC->AHB4ENR = RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN | RCC_AHB4ENR_GPIOGEN | RCC_AHB4ENR_GPIOHEN;
    GPIOA->MODER = 0; GPIOB->MODER = 0; GPIOC->MODER = 0; GPIOD->MODER = 0; GPIOE->MODER = 0; GPIOF->MODER = 0; GPIOG->MODER = 0; GPIOH->MODER = 0;
    GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0; GPIOD->ODR = 0; GPIOE->ODR = 0; GPIOF->ODR = 0; GPIOG->ODR = 0; GPIOH->ODR = 0;
    GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0; GPIOD->PUPDR = 0; GPIOE->PUPDR = 0; GPIOF->PUPDR = 0; GPIOG->PUPDR = 0; GPIOH->PUPDR = 0;
  #else
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    GPIOA->MODER = 0; GPIOB->MODER = 0; GPIOC->MODER = 0;
    GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0;
    GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0;
  #endif

  detect_configuration();
  detect_board_type();

  if (enter_bootloader_mode == ENTER_BOOTLOADER_MAGIC) {
  #ifdef PANDA
    current_board->set_gps_mode(GPS_DISABLED);
  #endif
    current_board->set_led(LED_GREEN, 1);
    jump_to_bootloader();
  }
}
