#ifdef STM32F4
  #include "stm32fx/inc/stm32f4xx.h"
  #include "stm32fx/inc/stm32f4xx_hal_gpio_ex.h"
  #define MCU_IDCODE 0x463U
  #define BCDDEVICE 0x23
#else
  #include "stm32fx/inc/stm32f2xx.h"
  #include "stm32fx/inc/stm32f2xx_hal_gpio_ex.h"
  #define MCU_IDCODE 0x411U
  #define BCDDEVICE 0x22
#endif

#define CORE_FREQ 96U // in Mhz
//APB1 - 48Mhz, APB2 - 96Mhz
#define APB1_FREQ CORE_FREQ/2U 
#define APB2_FREQ CORE_FREQ/1U

#define BOOTLOADER_ADDRESS 0x1FFF0004

// Around (1Mbps / 8 bits/byte / 12 bytes per message)
#define CAN_INTERRUPT_RATE 12000U

#define MAX_LED_FADE 8192U
#define LED_FADE_SHIFT 4U

// Threshold voltage (mV) for either of the SBUs to be below before deciding harness is connected
#define HARNESS_CONNECTED_THRESHOLD 2500U

#define NUM_INTERRUPTS 102U                // There are 102 external interrupt sources (see stm32f413.h)

#define TICK_TIMER_IRQ TIM1_BRK_TIM9_IRQn
#define TICK_TIMER TIM9

#define MICROSECOND_TIMER TIM2

#define INTERRUPT_TIMER_IRQ TIM6_DAC_IRQn
#define INTERRUPT_TIMER TIM6

#define PROVISION_CHUNK_ADDRESS 0x1FFF79E0
#define SERIAL_NUMBER_ADDRESS 0x1FFF79C0

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
#include "drivers/timers.h"

#ifndef BOOTSTUB
  #include "stm32fx/llcan.h"
#endif

#include "stm32fx/lladc.h"
#include "stm32fx/board.h"
#include "stm32fx/clock.h"

#if defined(PANDA) || defined(BOOTSTUB) || defined(PEDAL_USB)
  #include "stm32fx/llusb.h"
#endif

void early_gpio_float(void) {
  RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

  GPIOA->MODER = 0; GPIOB->MODER = 0; GPIOC->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0;
}

void handle_interrupt(IRQn_Type irq_type);
// Specific interrupts for STM32F413
void CAN1_TX_IRQHandler(void) {handle_interrupt(CAN1_TX_IRQn);}
void CAN1_RX0_IRQHandler(void) {handle_interrupt(CAN1_RX0_IRQn);}
void CAN1_RX1_IRQHandler(void) {handle_interrupt(CAN1_RX1_IRQn);}
void CAN1_SCE_IRQHandler(void) {handle_interrupt(CAN1_SCE_IRQn);}
void OTG_FS_WKUP_IRQHandler(void) {handle_interrupt(OTG_FS_WKUP_IRQn);}
void FSMC_IRQHandler(void) {handle_interrupt(FSMC_IRQn);}
void SDIO_IRQHandler(void) {handle_interrupt(SDIO_IRQn);}
void CAN2_TX_IRQHandler(void) {handle_interrupt(CAN2_TX_IRQn);}
void CAN2_RX0_IRQHandler(void) {handle_interrupt(CAN2_RX0_IRQn);}
void CAN2_RX1_IRQHandler(void) {handle_interrupt(CAN2_RX1_IRQn);}
void CAN2_SCE_IRQHandler(void) {handle_interrupt(CAN2_SCE_IRQn);}
void OTG_FS_IRQHandler(void) {handle_interrupt(OTG_FS_IRQn);}
#ifdef STM32F4
  void DFSDM1_FLT0_IRQHandler(void) {handle_interrupt(DFSDM1_FLT0_IRQn);}
  void DFSDM1_FLT1_IRQHandler(void) {handle_interrupt(DFSDM1_FLT1_IRQn);}
  void CAN3_TX_IRQHandler(void) {handle_interrupt(CAN3_TX_IRQn);}
  void CAN3_RX0_IRQHandler(void) {handle_interrupt(CAN3_RX0_IRQn);}
  void CAN3_RX1_IRQHandler(void) {handle_interrupt(CAN3_RX1_IRQn);}
  void CAN3_SCE_IRQHandler(void) {handle_interrupt(CAN3_SCE_IRQn);}
  void RNG_IRQHandler(void) {handle_interrupt(RNG_IRQn);}
  void FPU_IRQHandler(void) {handle_interrupt(FPU_IRQn);}
  void UART7_IRQHandler(void) {handle_interrupt(UART7_IRQn);}
  void UART8_IRQHandler(void) {handle_interrupt(UART8_IRQn);}
  void SPI4_IRQHandler(void) {handle_interrupt(SPI4_IRQn);}
  void SPI5_IRQHandler(void) {handle_interrupt(SPI5_IRQn);}
  void SAI1_IRQHandler(void) {handle_interrupt(SAI1_IRQn);}
  void UART9_IRQHandler(void) {handle_interrupt(UART9_IRQn);}
  void UART10_IRQHandler(void) {handle_interrupt(UART10_IRQn);}
  void QUADSPI_IRQHandler(void) {handle_interrupt(QUADSPI_IRQn);}
  void FMPI2C1_EV_IRQHandler(void) {handle_interrupt(FMPI2C1_EV_IRQn);}
  void FMPI2C1_ER_IRQHandler(void) {handle_interrupt(FMPI2C1_ER_IRQn);}
  void LPTIM1_IRQHandler(void) {handle_interrupt(LPTIM1_IRQn);}
  void DFSDM2_FLT0_IRQHandler(void) {handle_interrupt(DFSDM2_FLT0_IRQn);}
  void DFSDM2_FLT1_IRQHandler(void) {handle_interrupt(DFSDM2_FLT1_IRQn);}
  void DFSDM2_FLT2_IRQHandler(void) {handle_interrupt(DFSDM2_FLT2_IRQn);}
  void DFSDM2_FLT3_IRQHandler(void) {handle_interrupt(DFSDM2_FLT3_IRQn);}
#endif
