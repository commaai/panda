// ********************* Bare interrupt handlers *********************
// Interrupts for STM32H7x5
#pragma once
#include "drivers/interrupts.h"

static inline void WWDG_IRQHandler(void) {handle_interrupt(WWDG_IRQn);}
static inline void PVD_AVD_IRQHandler(void) {handle_interrupt(PVD_AVD_IRQn);}
static inline void TAMP_STAMP_IRQHandler(void) {handle_interrupt(TAMP_STAMP_IRQn);}
static inline void RTC_WKUP_IRQHandler(void) {handle_interrupt(RTC_WKUP_IRQn);}
static inline void FLASH_IRQHandler(void) {handle_interrupt(FLASH_IRQn);}
static inline void RCC_IRQHandler(void) {handle_interrupt(RCC_IRQn);}
static inline void EXTI0_IRQHandler(void) {handle_interrupt(EXTI0_IRQn);}
static inline void EXTI1_IRQHandler(void) {handle_interrupt(EXTI1_IRQn);}
static inline void EXTI2_IRQHandler(void) {handle_interrupt(EXTI2_IRQn);}
static inline void EXTI3_IRQHandler(void) {handle_interrupt(EXTI3_IRQn);}
static inline void EXTI4_IRQHandler(void) {handle_interrupt(EXTI4_IRQn);}
static inline void DMA1_Stream0_IRQHandler(void) {handle_interrupt(DMA1_Stream0_IRQn);}
static inline void DMA1_Stream1_IRQHandler(void) {handle_interrupt(DMA1_Stream1_IRQn);}
static inline void DMA1_Stream2_IRQHandler(void) {handle_interrupt(DMA1_Stream2_IRQn);}
static inline void DMA1_Stream3_IRQHandler(void) {handle_interrupt(DMA1_Stream3_IRQn);}
static inline void DMA1_Stream4_IRQHandler(void) {handle_interrupt(DMA1_Stream4_IRQn);}
static inline void DMA1_Stream5_IRQHandler(void) {handle_interrupt(DMA1_Stream5_IRQn);}
static inline void DMA1_Stream6_IRQHandler(void) {handle_interrupt(DMA1_Stream6_IRQn);}
static inline void ADC_IRQHandler(void) {handle_interrupt(ADC_IRQn);}
static inline void FDCAN1_IT0_IRQHandler(void) {handle_interrupt(FDCAN1_IT0_IRQn);}
static inline void FDCAN2_IT0_IRQHandler(void) {handle_interrupt(FDCAN2_IT0_IRQn);}
static inline void FDCAN1_IT1_IRQHandler(void) {handle_interrupt(FDCAN1_IT1_IRQn);}
static inline void FDCAN2_IT1_IRQHandler(void) {handle_interrupt(FDCAN2_IT1_IRQn);}
static inline void EXTI9_5_IRQHandler(void) {handle_interrupt(EXTI9_5_IRQn);}
static inline void TIM1_BRK_IRQHandler(void) {handle_interrupt(TIM1_BRK_IRQn);}
static inline void TIM1_UP_TIM10_IRQHandler(void) {handle_interrupt(TIM1_UP_TIM10_IRQn);}
static inline void TIM1_TRG_COM_IRQHandler(void) {handle_interrupt(TIM1_TRG_COM_IRQn);}
static inline void TIM1_CC_IRQHandler(void) {handle_interrupt(TIM1_CC_IRQn);}
static inline void TIM2_IRQHandler(void) {handle_interrupt(TIM2_IRQn);}
static inline void TIM3_IRQHandler(void) {handle_interrupt(TIM3_IRQn);}
static inline void TIM4_IRQHandler(void) {handle_interrupt(TIM4_IRQn);}
static inline void I2C1_EV_IRQHandler(void) {handle_interrupt(I2C1_EV_IRQn);}
static inline void I2C1_ER_IRQHandler(void) {handle_interrupt(I2C1_ER_IRQn);}
static inline void I2C2_EV_IRQHandler(void) {handle_interrupt(I2C2_EV_IRQn);}
static inline void I2C2_ER_IRQHandler(void) {handle_interrupt(I2C2_ER_IRQn);}
static inline void SPI1_IRQHandler(void) {handle_interrupt(SPI1_IRQn);}
static inline void SPI2_IRQHandler(void) {handle_interrupt(SPI2_IRQn);}
static inline void USART1_IRQHandler(void) {handle_interrupt(USART1_IRQn);}
static inline void USART2_IRQHandler(void) {handle_interrupt(USART2_IRQn);}
static inline void USART3_IRQHandler(void) {handle_interrupt(USART3_IRQn);}
static inline void EXTI15_10_IRQHandler(void) {handle_interrupt(EXTI15_10_IRQn);}
static inline void RTC_Alarm_IRQHandler(void) {handle_interrupt(RTC_Alarm_IRQn);}
static inline void TIM8_BRK_TIM12_IRQHandler(void) {handle_interrupt(TIM8_BRK_TIM12_IRQn);}
static inline void TIM8_UP_TIM13_IRQHandler(void) {handle_interrupt(TIM8_UP_TIM13_IRQn);}
static inline void TIM8_TRG_COM_TIM14_IRQHandler(void) {handle_interrupt(TIM8_TRG_COM_TIM14_IRQn);}
static inline void TIM8_CC_IRQHandler(void) {handle_interrupt(TIM8_CC_IRQn);}
static inline void DMA1_Stream7_IRQHandler(void) {handle_interrupt(DMA1_Stream7_IRQn);}
static inline void FMC_IRQHandler(void) {handle_interrupt(FMC_IRQn);}
static inline void SDMMC1_IRQHandler(void) {handle_interrupt(SDMMC1_IRQn);}
static inline void TIM5_IRQHandler(void) {handle_interrupt(TIM5_IRQn);}
static inline void SPI3_IRQHandler(void) {handle_interrupt(SPI3_IRQn);}
static inline void UART4_IRQHandler(void) {handle_interrupt(UART4_IRQn);}
static inline void UART5_IRQHandler(void) {handle_interrupt(UART5_IRQn);}
static inline void TIM6_DAC_IRQHandler(void) {handle_interrupt(TIM6_DAC_IRQn);}
static inline void TIM7_IRQHandler(void) {handle_interrupt(TIM7_IRQn);}
static inline void DMA2_Stream0_IRQHandler(void) {handle_interrupt(DMA2_Stream0_IRQn);}
static inline void DMA2_Stream1_IRQHandler(void) {handle_interrupt(DMA2_Stream1_IRQn);}
static inline void DMA2_Stream2_IRQHandler(void) {handle_interrupt(DMA2_Stream2_IRQn);}
static inline void DMA2_Stream3_IRQHandler(void) {handle_interrupt(DMA2_Stream3_IRQn);}
static inline void DMA2_Stream4_IRQHandler(void) {handle_interrupt(DMA2_Stream4_IRQn);}
static inline void ETH_IRQHandler(void) {handle_interrupt(ETH_IRQn);}
static inline void ETH_WKUP_IRQHandler(void) {handle_interrupt(ETH_WKUP_IRQn);}
static inline void FDCAN_CAL_IRQHandler(void) {handle_interrupt(FDCAN_CAL_IRQn);}
static inline void DMA2_Stream5_IRQHandler(void) {handle_interrupt(DMA2_Stream5_IRQn);}
static inline void DMA2_Stream6_IRQHandler(void) {handle_interrupt(DMA2_Stream6_IRQn);}
static inline void DMA2_Stream7_IRQHandler(void) {handle_interrupt(DMA2_Stream7_IRQn);}
static inline void USART6_IRQHandler(void) {handle_interrupt(USART6_IRQn);}
static inline void I2C3_EV_IRQHandler(void) {handle_interrupt(I2C3_EV_IRQn);}
static inline void I2C3_ER_IRQHandler(void) {handle_interrupt(I2C3_ER_IRQn);}
static inline void OTG_HS_EP1_OUT_IRQHandler(void) {handle_interrupt(OTG_HS_EP1_OUT_IRQn);}
static inline void OTG_HS_EP1_IN_IRQHandler(void) {handle_interrupt(OTG_HS_EP1_IN_IRQn);}
static inline void OTG_HS_WKUP_IRQHandler(void) {handle_interrupt(OTG_HS_WKUP_IRQn);}
static inline void OTG_HS_IRQHandler(void) {handle_interrupt(OTG_HS_IRQn);}
static inline void DCMI_PSSI_IRQHandler(void) {handle_interrupt(DCMI_PSSI_IRQn);}
static inline void CRYP_IRQHandler(void) {handle_interrupt(CRYP_IRQn);}
static inline void HASH_RNG_IRQHandler(void) {handle_interrupt(HASH_RNG_IRQn);}
static inline void FPU_IRQHandler(void) {handle_interrupt(FPU_IRQn);}
static inline void UART7_IRQHandler(void) {handle_interrupt(UART7_IRQn);}
static inline void UART8_IRQHandler(void) {handle_interrupt(UART8_IRQn);}
static inline void SPI4_IRQHandler(void) {handle_interrupt(SPI4_IRQn);}
static inline void SPI5_IRQHandler(void) {handle_interrupt(SPI5_IRQn);}
static inline void SPI6_IRQHandler(void) {handle_interrupt(SPI6_IRQn);}
static inline void SAI1_IRQHandler(void) {handle_interrupt(SAI1_IRQn);}
static inline void LTDC_IRQHandler(void) {handle_interrupt(LTDC_IRQn);}
static inline void LTDC_ER_IRQHandler(void) {handle_interrupt(LTDC_ER_IRQn);}
static inline void DMA2D_IRQHandler(void) {handle_interrupt(DMA2D_IRQn);}
static inline void OCTOSPI1_IRQHandler(void) {handle_interrupt(OCTOSPI1_IRQn);}
static inline void LPTIM1_IRQHandler(void) {handle_interrupt(LPTIM1_IRQn);}
static inline void CEC_IRQHandler(void) {handle_interrupt(CEC_IRQn);}
static inline void I2C4_EV_IRQHandler(void) {handle_interrupt(I2C4_EV_IRQn);}
static inline void I2C4_ER_IRQHandler(void) {handle_interrupt(I2C4_ER_IRQn);}
static inline void SPDIF_RX_IRQHandler(void) {handle_interrupt(SPDIF_RX_IRQn);}
static inline void DMAMUX1_OVR_IRQHandler(void) {handle_interrupt(DMAMUX1_OVR_IRQn);}
static inline void DFSDM1_FLT0_IRQHandler(void) {handle_interrupt(DFSDM1_FLT0_IRQn);}
static inline void DFSDM1_FLT1_IRQHandler(void) {handle_interrupt(DFSDM1_FLT1_IRQn);}
static inline void DFSDM1_FLT2_IRQHandler(void) {handle_interrupt(DFSDM1_FLT2_IRQn);}
static inline void DFSDM1_FLT3_IRQHandler(void) {handle_interrupt(DFSDM1_FLT3_IRQn);}
static inline void SWPMI1_IRQHandler(void) {handle_interrupt(SWPMI1_IRQn);}
static inline void TIM15_IRQHandler(void) {handle_interrupt(TIM15_IRQn);}
static inline void TIM16_IRQHandler(void) {handle_interrupt(TIM16_IRQn);}
static inline void TIM17_IRQHandler(void) {handle_interrupt(TIM17_IRQn);}
static inline void MDIOS_WKUP_IRQHandler(void) {handle_interrupt(MDIOS_WKUP_IRQn);}
static inline void MDIOS_IRQHandler(void) {handle_interrupt(MDIOS_IRQn);}
static inline void MDMA_IRQHandler(void) {handle_interrupt(MDMA_IRQn);}
static inline void SDMMC2_IRQHandler(void) {handle_interrupt(SDMMC2_IRQn);}
static inline void HSEM1_IRQHandler(void) {handle_interrupt(HSEM1_IRQn);}
static inline void ADC3_IRQHandler(void) {handle_interrupt(ADC3_IRQn);}
static inline void DMAMUX2_OVR_IRQHandler(void) {handle_interrupt(DMAMUX2_OVR_IRQn);}
static inline void BDMA_Channel0_IRQHandler(void) {handle_interrupt(BDMA_Channel0_IRQn);}
static inline void BDMA_Channel1_IRQHandler(void) {handle_interrupt(BDMA_Channel1_IRQn);}
static inline void BDMA_Channel2_IRQHandler(void) {handle_interrupt(BDMA_Channel2_IRQn);}
static inline void BDMA_Channel3_IRQHandler(void) {handle_interrupt(BDMA_Channel3_IRQn);}
static inline void BDMA_Channel4_IRQHandler(void) {handle_interrupt(BDMA_Channel4_IRQn);}
static inline void BDMA_Channel5_IRQHandler(void) {handle_interrupt(BDMA_Channel5_IRQn);}
static inline void BDMA_Channel6_IRQHandler(void) {handle_interrupt(BDMA_Channel6_IRQn);}
static inline void BDMA_Channel7_IRQHandler(void) {handle_interrupt(BDMA_Channel7_IRQn);}
static inline void COMP_IRQHandler(void) {handle_interrupt(COMP_IRQn);}
static inline void LPTIM2_IRQHandler(void) {handle_interrupt(LPTIM2_IRQn);}
static inline void LPTIM3_IRQHandler(void) {handle_interrupt(LPTIM3_IRQn);}
static inline void LPTIM4_IRQHandler(void) {handle_interrupt(LPTIM4_IRQn);}
static inline void LPTIM5_IRQHandler(void) {handle_interrupt(LPTIM5_IRQn);}
static inline void LPUART1_IRQHandler(void) {handle_interrupt(LPUART1_IRQn);}
static inline void CRS_IRQHandler(void) {handle_interrupt(CRS_IRQn);}
static inline void ECC_IRQHandler(void) {handle_interrupt(ECC_IRQn);}
static inline void SAI4_IRQHandler(void) {handle_interrupt(SAI4_IRQn);}
static inline void DTS_IRQHandler(void) {handle_interrupt(DTS_IRQn);}
static inline void WAKEUP_PIN_IRQHandler(void) {handle_interrupt(WAKEUP_PIN_IRQn);}
static inline void OCTOSPI2_IRQHandler(void) {handle_interrupt(OCTOSPI2_IRQn);}
static inline void OTFDEC1_IRQHandler(void) {handle_interrupt(OTFDEC1_IRQn);}
static inline void OTFDEC2_IRQHandler(void) {handle_interrupt(OTFDEC2_IRQn);}
static inline void FMAC_IRQHandler(void) {handle_interrupt(FMAC_IRQn);}
static inline void CORDIC_IRQHandler(void) {handle_interrupt(CORDIC_IRQn);}
static inline void UART9_IRQHandler(void) {handle_interrupt(UART9_IRQn);}
static inline void USART10_IRQHandler(void) {handle_interrupt(USART10_IRQn);}
static inline void I2C5_EV_IRQHandler(void) {handle_interrupt(I2C5_EV_IRQn);}
static inline void I2C5_ER_IRQHandler(void) {handle_interrupt(I2C5_ER_IRQn);}
static inline void FDCAN3_IT0_IRQHandler(void) {handle_interrupt(FDCAN3_IT0_IRQn);}
static inline void FDCAN3_IT1_IRQHandler(void) {handle_interrupt(FDCAN3_IT1_IRQn);}
static inline void TIM23_IRQHandler(void) {handle_interrupt(TIM23_IRQn);}
static inline void TIM24_IRQHandler(void) {handle_interrupt(TIM24_IRQn);}
