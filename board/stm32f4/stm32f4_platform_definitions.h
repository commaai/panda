#pragma once
#include "stm32f4/inc/stm32f4xx.h"
#include "stm32f4/inc/stm32f4xx_hal_gpio_ex.h"
#define MCU_IDCODE 0x463U

#define CORE_FREQ 96U // in MHz
#define APB1_FREQ (CORE_FREQ / 2U)
#define APB1_TIMER_FREQ                                                        \
  (APB1_FREQ * 2U) // APB1 is multiplied by 2 for the timer peripherals
#define APB2_FREQ (CORE_FREQ / 2U)
#define APB2_TIMER_FREQ                                                        \
  (APB2_FREQ * 2U) // APB2 is multiplied by 2 for the timer peripherals

#define BOOTLOADER_ADDRESS 0x1FFF0004U

// Around (1Mbps / 8 bits/byte / 12 bytes per message)
#define CAN_INTERRUPT_RATE 12000U

#define MAX_LED_FADE 8192U

#define NUM_INTERRUPTS                                                         \
  102U // There are 102 external interrupt sources (see stm32f413.h)

#define TICK_TIMER_IRQ TIM1_BRK_TIM9_IRQn
#define TICK_TIMER TIM9

#define MICROSECOND_TIMER TIM2

#define INTERRUPT_TIMER_IRQ TIM6_DAC_IRQn
#define INTERRUPT_TIMER TIM6

#define IND_WDG IWDG

#define PROVISION_CHUNK_ADDRESS 0x1FFF79E0U
#define DEVICE_SERIAL_NUMBER_ADDRESS 0x1FFF79C0U
