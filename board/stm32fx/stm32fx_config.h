#ifdef STM32F4
  #include "stm32fx/inc/stm32f4xx.h"
  #include "stm32fx/inc/stm32f4xx_hal_gpio_ex.h"
  const uint16_t MCU_IDCODE = 0x463;
  const uint8_t BCDDEVICE = 0x23;
#else
  #include "stm32fx/inc/stm32f2xx.h"
  #include "stm32fx/inc/stm32f2xx_hal_gpio_ex.h"
  const uint16_t MCU_IDCODE = 0x411;
  const uint8_t BCDDEVICE = 0x22;
#endif

#define CORE_FREQ 96U // in Mhz
//APB1 - 48Mhz, APB2 - 96Mhz
#define APB1_FREQ CORE_FREQ/2U 
#define APB2_FREQ CORE_FREQ/1U

#define TICK_TIMER_IRQ TIM1_BRK_TIM9_IRQn
#define TICK_TIMER TIM9

#define MICROSECOND_TIMER TIM2

#define INTERRUPT_TIMER_IRQ TIM6_DAC_IRQn
#define INTERRUPT_TIMER TIM6
// There are 102 external interrupt sources (see stm32f413.h)
#define NUM_INTERRUPTS 102

// from the linker script
const uint32_t APP_START_ADDRESS = 0x8004000;

const uint32_t BOOTLOADER_ADDRESS = 0x1FFF0004;
const uint32_t PROVISION_CHUNK_ADDRESS = 0x1FFF79E0;
const uint32_t DEVICE_SERIAL_NUMBER_ADDRESS = 0x1FFF79C0;

// Around (1Mbps / 8 bits/byte / 12 bytes per message)
const uint16_t CAN_INTERRUPT_RATE = 12000;
const uint16_t MAX_LED_FADE = 8192;
// Threshold voltage (mV) for either of the SBUs to be below before deciding harness is connected
const uint16_t HARNESS_CONNECTED_THRESHOLD = 2500;

#ifndef BOOTSTUB
  #ifdef PANDA
    #include "main_declarations.h"
  #else
    #include "pedal/main_declarations.h"
  #endif
#else
  #include "bootstub_declarations.h"
#endif

#include "libc.h"
#include "critical.h"
#include "faults.h"

#include "drivers/registers.h"
#include "drivers/interrupts.h"
#include "drivers/gpio.h"
#include "stm32fx/peripherals.h"
#include "stm32fx/interrupt_handlers.h"
#include "drivers/timers.h"
#include "stm32fx/lladc.h"
#include "stm32fx/board.h"
#include "stm32fx/clock.h"

#if !defined (BOOTSTUB) && (defined(PANDA) || defined(PEDAL_USB))
  #include "drivers/uart.h"
  #include "stm32fx/lluart.h"
#endif

#ifndef BOOTSTUB
  #include "stm32fx/llcan.h"
#endif

#if defined(PANDA) || defined(BOOTSTUB) || defined(PEDAL_USB)
  #include "stm32fx/llusb.h"
#endif

void early_gpio_float(void) {
  RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

  GPIOA->MODER = 0; GPIOB->MODER = 0; GPIOC->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0;
}
