#ifndef STM32H7_CONFIG_H
#define STM32H7_CONFIG_H

#ifdef STM32H7
#include "stm32h7xx.h"
#endif

#include "board/drivers/driver_declarations.h"
#include "stm32h7xx_hal_gpio_ex.h"
#include "board/utils.h"

#define MCU_IDCODE 0x483U
#define CORE_FREQ 240U // in Mhz
//APB1 - 120Mhz, APB2 - 120Mhz
#define APB1_FREQ (CORE_FREQ/4U)
#define APB1_TIMER_FREQ (APB1_FREQ*2U) // APB1 is multiplied by 2 for the timer peripherals
#define APB2_FREQ (CORE_FREQ/4U)
#define APB2_TIMER_FREQ (APB2_FREQ*2U) // APB2 is multiplied by 2 for the timer peripherals
#define BOOTLOADER_ADDRESS 0x1FF09804U
#define NUM_INTERRUPTS 163U

#define CAN_INTERRUPT_RATE 16000U
#define MAX_LED_FADE 10240U

#define TICK_TIMER_IRQ TIM8_BRK_TIM12_IRQn
#define TICK_TIMER TIM12
#define MICROSECOND_TIMER TIM2
#define INTERRUPT_TIMER_IRQ TIM6_DAC_IRQn
#define INTERRUPT_TIMER TIM6
#define IND_WDG IWDG1

#define PROVISION_CHUNK_ADDRESS 0x080FFFE0U
#define DEVICE_SERIAL_NUMBER_ADDRESS 0x080FFFC0U

#include "board/libc.h"
#include "board/sys/critical.h"
#include "board/sys/faults.h"
#include "board/comms_definitions.h"

#ifndef BOOTSTUB
  #include "board/main_definitions.h"
#else
  #include "board/bootstub_declarations.h"
#endif

// Basic drivers must be included before board.h
#include "board/drivers/registers.h"
#include "board/drivers/interrupts.h"
#include "board/drivers/gpio.h"
#include "board/drivers/pwm.h"
#include "board/drivers/led.h"
#include "board/drivers/uart.h"
#include "board/stm32h7/lluart.h"

#ifdef PANDA_JUNGLE
#include "board/jungle/stm32h7/board.h"
#elif defined(PANDA_BODY)
#include "board/body/stm32h7/board.h"
#else
#include "board/stm32h7/board.h"
#endif

#include "board/drivers/drivers.h"
#include "board/stm32h7/peripherals.h"
#include "board/stm32h7/interrupt_handlers.h"
#include "board/drivers/timers.h"

#include "board/stm32h7/clock.h"

#ifdef BOOTSTUB
  #include "board/stm32h7/llflash.h"
#else
  #include "board/stm32h7/llfdcan.h"
#endif

#include "board/stm32h7/llusb.h"
#include "board/drivers/spi.h"
#include "board/stm32h7/llspi.h"

#endif
