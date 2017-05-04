#define ENTER_BOOTLOADER_MAGIC 0xdeadbeef
#define POST_BOOTLOADER_MAGIC 0xdeadb111
extern uint32_t enter_bootloader_mode;
extern void *_app_start[];
void *g_pfnVectors;
void spi_flasher();

int has_external_debug_serial = 0;
int is_giant_panda = 0;
#define PULL_EFFECTIVE_DELAY 10

// must call again from main because BSS is zeroed
inline void detect() {
  volatile int i;
  // detect has_external_debug_serial
  GPIOA->PUPDR |= GPIO_PUPDR_PUPDR3_1;
  for (i=0;i<PULL_EFFECTIVE_DELAY;i++);
  has_external_debug_serial = (GPIOA->IDR & (1 << 3)) == (1 << 3);
  
  // detect is_giant_panda
  is_giant_panda = 0;
#ifdef PANDA
  GPIOB->PUPDR |= GPIO_PUPDR_PUPDR1_1;
  for (i=0;i<PULL_EFFECTIVE_DELAY;i++);
  is_giant_panda = (GPIOB->IDR & (1 << 1)) == (1 << 1);
#endif
}

inline void early() {
  // after it's been in the bootloader, things are initted differently, so we reset
  if (enter_bootloader_mode == POST_BOOTLOADER_MAGIC) {
    enter_bootloader_mode = 0;
    NVIC_SystemReset();
  }

  volatile int i;
  // if wrong chip, reboot
  volatile unsigned int id = DBGMCU->IDCODE;
  #ifdef STM32F4
    if ((id&0xFFF) != 0x463) enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
  #else
    if ((id&0xFFF) != 0x411) enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
  #endif

  // setup interrupt table
  SCB->VTOR = (uint32_t)&g_pfnVectors;

  // early GPIOs
  RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
  GPIOA->MODER = 0; GPIOB->MODER = 0; GPIOC->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0;

  detect();
  
  #ifdef PANDA
    // enable the ESP, disable ESP boot mode
    // unless we are on a giant panda, then there's no ESP
    if (!is_giant_panda) {
      GPIOC->ODR = (1 << 14) | (1 << 5);
    }

    // these are outputs to control the ESP
    GPIOC->MODER = GPIO_MODER_MODER14_0 | GPIO_MODER_MODER5_0;

    // check if the ESP is trying to put me in boot mode
    // enable pull up
    GPIOB->PUPDR |= GPIO_PUPDR_PUPDR0_0;
    for (i=0;i<PULL_EFFECTIVE_DELAY;i++);
    
    #ifdef BOOTSTUB
      // if it's driven low, jump to spi flasher
      if (!(GPIOB->IDR & 1)) {
        spi_flasher();
      }
    #endif

  #endif

  if (enter_bootloader_mode == ENTER_BOOTLOADER_MAGIC) {
    // ESP OFF
    GPIOC->ODR &= ~((1 << 14) | (1 << 5));

    // green LED on
    // sadly, on the NEO board the bootloader turns it off
    #ifdef PANDA
      GPIOC->MODER |= GPIO_MODER_MODER7_0;
      GPIOC->ODR &= ~(1 << (6 + 1));
    #else
      GPIOB->MODER |= GPIO_MODER_MODER11_0;
      GPIOB->ODR &= ~(1 << (10 + 1));
    #endif

    // do enter bootloader
    enter_bootloader_mode = POST_BOOTLOADER_MAGIC;
    void (*bootloader)(void) = (void (*)(void)) (*((uint32_t *)0x1fff0004));

    // jump to bootloader
    bootloader();

    // LOOP
    while(1);
  }
}

