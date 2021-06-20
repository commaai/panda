#ifdef STM32F4
  #include "stm32f4xx.h"
  #include "stm32f4xx_hal_gpio_ex.h"
  #define MCU_IDCODE 0x463U
#else
  #include "stm32f2xx.h"
  #include "stm32f2xx_hal_gpio_ex.h"
  #define MCU_IDCODE 0x411U
#endif

#define CORE_FREQ 96U // 96Mhz
//APB1 - 48Mhz, APB2 - 96Mhz
#define APB1_FREQ CORE_FREQ/2U 
#define APB2_FREQ CORE_FREQ/1U

#define BOOTLOADER_ADDRESS 0x1FFF0004U


// Around (1Mbps / 8 bits/byte / 12 bytes per message)
#define CAN_INTERRUPT_RATE 12000U

#define MAX_LED_FADE 8192U

#define NUM_INTERRUPTS 102U                // There are 102 external interrupt sources (see stm32f413.h)

#define TICK_TIMER_IRQ TIM1_BRK_TIM9_IRQn
#define TICK_TIMER TIM9

#define MICROSECOND_TIMER TIM2

#define PROVISION_CHUNK_ADDRESS 0x1FFF79E0U
#define SERIAL_NUMBER_ADDRESS 0x1FFF79C0U

void early_gpio_float(void) {
  RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

  GPIOA->MODER = 0; GPIOB->MODER = 0; GPIOC->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0;
}
