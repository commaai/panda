#include "stm32h7xx.h"
#include "stm32h7xx_hal_gpio_ex.h"
#define MCU_IDCODE 0x483U

#define CORE_FREQ 240U // in Mhz
//APB1 - 120Mhz, APB2 - 120Mhz
#define APB1_FREQ CORE_FREQ/2U 
#define APB2_FREQ CORE_FREQ/2U

#define BOOTLOADER_ADDRESS 0x1FF09804

// Around (1Mbps / 8 bits/byte / 12 bytes per message)
#define CAN_INTERRUPT_RATE 12000U // FIXME: should raise to 16000 ?

#define MAX_LED_FADE 4096U // FIXME: adjust as got rid of delay_us and delay_ms

#define NUM_INTERRUPTS 163U                // There are 163 external interrupt sources (see stm32f735xx.h)

#define TICK_TIMER_IRQ TIM8_BRK_TIM12_IRQn
#define TICK_TIMER TIM12

#define MICROSECOND_TIMER TIM2

#define INTERRUPT_TIMER_IRQ TIM6_DAC_IRQn
#define INTERRUPT_TIMER TIM6

#define PROVISION_CHUNK_ADDRESS 0x1FFF79E0 // FIXME: stm32h735 has no OTP section
#define SERIAL_NUMBER_ADDRESS 0x1FFF79C0

#ifndef BOOTSTUB
  #include "../main_declarations.h"
#else
  #include "../bootstub_declarations.h"
#endif

#include "../libc.h"
#include "../critical.h"
#include "../faults.h"

#include "../drivers/registers.h"
#include "../drivers/interrupts.h"
#include "../drivers/timers.h"
#include "../drivers/gpio.h"

#ifndef BOOTSTUB
  #include "llfdcan.h"
#endif

// FIXME: might need to move fdcan.h here also, need tests
#include "lladc.h"
#include "board.h"
#include "clock.h"
#include "llusb.h"

void early_gpio_float(void) {
  RCC->AHB4ENR = RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN | RCC_AHB4ENR_GPIOGEN | RCC_AHB4ENR_GPIOHEN;
  GPIOA->MODER = 0; GPIOB->MODER = 0; GPIOC->MODER = 0; GPIOD->MODER = 0; GPIOE->MODER = 0; GPIOF->MODER = 0; GPIOG->MODER = 0; GPIOH->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0; GPIOD->ODR = 0; GPIOE->ODR = 0; GPIOF->ODR = 0; GPIOG->ODR = 0; GPIOH->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0; GPIOD->PUPDR = 0; GPIOE->PUPDR = 0; GPIOF->PUPDR = 0; GPIOG->PUPDR = 0; GPIOH->PUPDR = 0;
}
