// ********************* Interrupt helpers *********************
volatile bool interrupts_enabled = false;

void enable_interrupts(void) {
  interrupts_enabled = true;
  __enable_irq();
}

void disable_interrupts(void) {
  interrupts_enabled = false;
  __disable_irq();
}

uint8_t global_critical_depth = 0U;
#define ENTER_CRITICAL()                                      \
  __disable_irq();                                            \
  global_critical_depth += 1U;

#define EXIT_CRITICAL()                                       \
  global_critical_depth -= 1U;                                \
  if ((global_critical_depth == 0U) && interrupts_enabled) {  \
    __enable_irq();                                           \
  }

// ********************* Interrupt handling *********************
void unused_interrupt_handler(void) {
  // Something is wrong if this handler is called!
  puts("Unused interrupt handler called!");
}

#define NUM_INTERRUPTS 102U                // There are 102 external interrupt sources (see stm32f413.h)
void (*interrupt_handlers[NUM_INTERRUPTS])(void);

#define REGISTER_INTERRUPT(irq_type, func_ptr) interrupt_handlers[irq_type] = func_ptr;

void handle_interrupt(IRQn_Type irq_type){
  // Call defined interrupt
  (*interrupt_handlers[irq_type])();
}

void init_interrupts(void){
  for(uint16_t i=0U; i<NUM_INTERRUPTS; i++){
    interrupt_handlers[i] = unused_interrupt_handler;
  }
}

// ********************* Bare interrupt handlers *********************
// Only implemented the STM32F413 interrupts for now, the STM32F203 does not fall into the scope of SIL2

 void WWDG_IRQHandler(void) {handle_interrupt(WWDG_IRQn);}
 void PVD_IRQHandler(void) {handle_interrupt(PVD_IRQn);}
 void TAMP_STAMP_IRQHandler(void) {handle_interrupt(TAMP_STAMP_IRQn);}
 void RTC_WKUP_IRQHandler(void) {handle_interrupt(RTC_WKUP_IRQn);}
 void FLASH_IRQHandler(void) {handle_interrupt(FLASH_IRQn);}
 void RCC_IRQHandler(void) {handle_interrupt(RCC_IRQn);}
 void EXTI0_IRQHandler(void) {handle_interrupt(EXTI0_IRQn);}
 void EXTI1_IRQHandler(void) {handle_interrupt(EXTI1_IRQn);}
 void EXTI2_IRQHandler(void) {handle_interrupt(EXTI2_IRQn);}
 void EXTI3_IRQHandler(void) {handle_interrupt(EXTI3_IRQn);}
 void EXTI4_IRQHandler(void) {handle_interrupt(EXTI4_IRQn);}
 void DMA1_Stream0_IRQHandler(void) {handle_interrupt(DMA1_Stream0_IRQn);}
 void DMA1_Stream1_IRQHandler(void) {handle_interrupt(DMA1_Stream1_IRQn);}
 void DMA1_Stream2_IRQHandler(void) {handle_interrupt(DMA1_Stream2_IRQn);}
 void DMA1_Stream3_IRQHandler(void) {handle_interrupt(DMA1_Stream3_IRQn);}
 void DMA1_Stream4_IRQHandler(void) {handle_interrupt(DMA1_Stream4_IRQn);}
 void DMA1_Stream5_IRQHandler(void) {handle_interrupt(DMA1_Stream5_IRQn);}
 void DMA1_Stream6_IRQHandler(void) {handle_interrupt(DMA1_Stream6_IRQn);}
 void ADC_IRQHandler(void) {handle_interrupt(ADC_IRQn);}
 void CAN1_TX_IRQHandler(void) {handle_interrupt(CAN1_TX_IRQn);}
 void CAN1_RX0_IRQHandler(void) {handle_interrupt(CAN1_RX0_IRQn);}
 void CAN1_RX1_IRQHandler(void) {handle_interrupt(CAN1_RX1_IRQn);}
 void CAN1_SCE_IRQHandler(void) {handle_interrupt(CAN1_SCE_IRQn);}
 void EXTI9_5_IRQHandler(void) {handle_interrupt(EXTI9_5_IRQn);}
 void TIM1_BRK_TIM9_IRQHandler(void) {handle_interrupt(TIM1_BRK_TIM9_IRQn);}
 void TIM1_UP_TIM10_IRQHandler(void) {handle_interrupt(TIM1_UP_TIM10_IRQn);}
 void TIM1_TRG_COM_TIM11_IRQHandler(void) {handle_interrupt(TIM1_TRG_COM_TIM11_IRQn);}
 void TIM1_CC_IRQHandler(void) {handle_interrupt(TIM1_CC_IRQn);}
 void TIM2_IRQHandler(void) {handle_interrupt(TIM2_IRQn);}
 void TIM3_IRQHandler(void) {handle_interrupt(TIM3_IRQn);}
 void TIM4_IRQHandler(void) {handle_interrupt(TIM4_IRQn);}
 void I2C1_EV_IRQHandler(void) {handle_interrupt(I2C1_EV_IRQn);}
 void I2C1_ER_IRQHandler(void) {handle_interrupt(I2C1_ER_IRQn);}
 void I2C2_EV_IRQHandler(void) {handle_interrupt(I2C2_EV_IRQn);}
 void I2C2_ER_IRQHandler(void) {handle_interrupt(I2C2_ER_IRQn);}
 void SPI1_IRQHandler(void) {handle_interrupt(SPI1_IRQn);}
 void SPI2_IRQHandler(void) {handle_interrupt(SPI2_IRQn);}
 void USART1_IRQHandler(void) {handle_interrupt(USART1_IRQn);}
 void USART2_IRQHandler(void) {handle_interrupt(USART2_IRQn);}
 void USART3_IRQHandler(void) {handle_interrupt(USART3_IRQn);}
 void EXTI15_10_IRQHandler(void) {handle_interrupt(EXTI15_10_IRQn);}
 void RTC_Alarm_IRQHandler(void) {handle_interrupt(RTC_Alarm_IRQn);}
 void OTG_FS_WKUP_IRQHandler(void) {handle_interrupt(OTG_FS_WKUP_IRQn);}
 void TIM8_BRK_TIM12_IRQHandler(void) {handle_interrupt(TIM8_BRK_TIM12_IRQn);}
 void TIM8_UP_TIM13_IRQHandler(void) {handle_interrupt(TIM8_UP_TIM13_IRQn);}
 void TIM8_TRG_COM_TIM14_IRQHandler(void) {handle_interrupt(TIM8_TRG_COM_TIM14_IRQn);}
 void TIM8_CC_IRQHandler(void) {handle_interrupt(TIM8_CC_IRQn);}
 void DMA1_Stream7_IRQHandler(void) {handle_interrupt(DMA1_Stream7_IRQn);}
 void FSMC_IRQHandler(void) {handle_interrupt(FSMC_IRQn);}
 void SDIO_IRQHandler(void) {handle_interrupt(SDIO_IRQn);}
 void TIM5_IRQHandler(void) {handle_interrupt(TIM5_IRQn);}
 void SPI3_IRQHandler(void) {handle_interrupt(SPI3_IRQn);}
 void UART4_IRQHandler(void) {handle_interrupt(UART4_IRQn);}
 void UART5_IRQHandler(void) {handle_interrupt(UART5_IRQn);}
 void TIM6_DAC_IRQHandler(void) {handle_interrupt(TIM6_DAC_IRQn);}
 void TIM7_IRQHandler(void) {handle_interrupt(TIM7_IRQn);}
 void DMA2_Stream0_IRQHandler(void) {handle_interrupt(DMA2_Stream0_IRQn);}
 void DMA2_Stream1_IRQHandler(void) {handle_interrupt(DMA2_Stream1_IRQn);}
 void DMA2_Stream2_IRQHandler(void) {handle_interrupt(DMA2_Stream2_IRQn);}
 void DMA2_Stream3_IRQHandler(void) {handle_interrupt(DMA2_Stream3_IRQn);}
 void DMA2_Stream4_IRQHandler(void) {handle_interrupt(DMA2_Stream4_IRQn);}
 void CAN2_TX_IRQHandler(void) {handle_interrupt(CAN2_TX_IRQn);}
 void CAN2_RX0_IRQHandler(void) {handle_interrupt(CAN2_RX0_IRQn);}
 void CAN2_RX1_IRQHandler(void) {handle_interrupt(CAN2_RX1_IRQn);}
 void CAN2_SCE_IRQHandler(void) {handle_interrupt(CAN2_SCE_IRQn);}
 void OTG_FS_IRQHandler(void) {handle_interrupt(OTG_FS_IRQn);}
 void DMA2_Stream5_IRQHandler(void) {handle_interrupt(DMA2_Stream5_IRQn);}
 void DMA2_Stream6_IRQHandler(void) {handle_interrupt(DMA2_Stream6_IRQn);}
 void DMA2_Stream7_IRQHandler(void) {handle_interrupt(DMA2_Stream7_IRQn);}
 void USART6_IRQHandler(void) {handle_interrupt(USART6_IRQn);}
 void I2C3_EV_IRQHandler(void) {handle_interrupt(I2C3_EV_IRQn);}
 void I2C3_ER_IRQHandler(void) {handle_interrupt(I2C3_ER_IRQn);}
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