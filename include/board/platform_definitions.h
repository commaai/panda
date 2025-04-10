#pragma once

#ifdef STM32H7
 #include "stm32h7/inc/stm32h7xx.h"
 #include "stm32h7/inc/stm32h7xx_hal_gpio_ex.h"
 #define MCU_IDCODE 0x483U

 #define CORE_FREQ 240U // in Mhz
 //APB1 - 120Mhz, APB2 - 120Mhz
 #define APB1_FREQ (CORE_FREQ/4U)
 #define APB1_TIMER_FREQ (APB1_FREQ*2U) // APB1 is multiplied by 2 for the timer peripherals
 #define APB2_FREQ (CORE_FREQ/4U)
 #define APB2_TIMER_FREQ (APB2_FREQ*2U) // APB2 is multiplied by 2 for the timer peripherals

 #define BOOTLOADER_ADDRESS 0x1FF09804U

 /*
 An IRQ is received on message RX/TX (or RX errors), with
 separate IRQs for RX and TX.

 0-byte CAN FD frame as the worst case:
 - 17 slow bits = SOF + 11 ID + R1 + IDE + EDL + R0 + BRS
 - 23 fast bits = ESI + 4 DLC + 0 DATA + 17 CRC + CRC delimeter
 - 12 slow bits = ACK + DEL + 7 EOF + 3 IFS
 - all currently supported cars are 0.5 Mbps / 2 Mbps

 1 / ((29 bits / 0.5Mbps) + (23 bits / 2Mbps)) = 14388Hz
 */
 #define CAN_INTERRUPT_RATE 16000U

 #define MAX_LED_FADE 10240U

 // There are 163 external interrupt sources (see stm32f735xx.h)
 #define NUM_INTERRUPTS 163U

 #define TICK_TIMER_IRQ TIM8_BRK_TIM12_IRQn
 #define TICK_TIMER TIM12

 #define MICROSECOND_TIMER TIM2

 #define INTERRUPT_TIMER_IRQ TIM6_DAC_IRQn
 #define INTERRUPT_TIMER TIM6

 #define IND_WDG IWDG1

 #define PROVISION_CHUNK_ADDRESS 0x080FFFE0U
 #define DEVICE_SERIAL_NUMBER_ADDRESS 0x080FFFC0U

#elif defined(STM32F4)
 #include "stm32f4/inc/stm32f4xx.h"
 #include "stm32f4/inc/stm32f4xx_hal_gpio_ex.h"
 #define MCU_IDCODE 0x463U

 #define CORE_FREQ 96U // in MHz
 #define APB1_FREQ (CORE_FREQ/2U)
 #define APB1_TIMER_FREQ (APB1_FREQ*2U)  // APB1 is multiplied by 2 for the timer peripherals
 #define APB2_FREQ (CORE_FREQ/2U)
 #define APB2_TIMER_FREQ (APB2_FREQ*2U)  // APB2 is multiplied by 2 for the timer peripherals

 #define BOOTLOADER_ADDRESS 0x1FFF0004U

 // Around (1Mbps / 8 bits/byte / 12 bytes per message)
 #define CAN_INTERRUPT_RATE 12000U

 #define MAX_LED_FADE 8192U

 #define NUM_INTERRUPTS 102U                // There are 102 external interrupt sources (see stm32f413.h)

 #define TICK_TIMER_IRQ TIM1_BRK_TIM9_IRQn
 #define TICK_TIMER TIM9

 #define MICROSECOND_TIMER TIM2

 #define INTERRUPT_TIMER_IRQ TIM6_DAC_IRQn
 #define INTERRUPT_TIMER TIM6

 #define IND_WDG IWDG

 #define PROVISION_CHUNK_ADDRESS 0x1FFF79E0U
 #define DEVICE_SERIAL_NUMBER_ADDRESS 0x1FFF79C0U
#endif
