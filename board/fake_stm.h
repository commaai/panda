// minimal code to fake a panda for tests
#ifndef FAKE_STM_H
#define FAKE_STM_H

// cppcheck-suppress misra-c2012-21.6 ; host-side testing only
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
  uint32_t CNT;
  uint32_t ARR;
  uint32_t PSC;
  uint32_t CR1;
  uint32_t CR2;
  uint32_t SR;
  uint32_t DIER;
  uint32_t CCMR1;
  uint32_t CCMR2;
  uint32_t CCER;
  uint32_t CCR1;
  uint32_t CCR2;
  uint32_t CCR3;
  uint32_t CCR4;
  uint32_t BDTR;
  uint32_t SMCR;
} TIM_TypeDef;

typedef struct {
  uint32_t MODER;
  uint32_t OTYPER;
  uint32_t OSPEEDR;
  uint32_t PUPDR;
  uint32_t IDR;
  uint32_t ODR;
  uint32_t BSRR;
  uint32_t LCKR;
  uint32_t AFR[2];
} GPIO_TypeDef;

typedef struct {
  uint32_t ISR;
  uint32_t ICR;
  uint32_t RDR;
  uint32_t TDR;
  uint32_t BRR;
  uint32_t CR1;
  uint32_t CR2;
  uint32_t CR3;
} USART_TypeDef;

typedef struct {
  uint32_t CCCR;
  uint32_t TEST;
  uint32_t ECR;
  uint32_t TXBRP;
  uint32_t RXF0S;
  uint32_t RXF1S;
} FDCAN_GlobalTypeDef;

typedef struct {
  uint32_t CR;
  uint32_t ISR;
  uint32_t DR;
  uint32_t SQR1;
} ADC_TypeDef;

typedef struct {
  uint32_t CCR;
} DMAMUX_Channel_TypeDef;

typedef struct {
  uint32_t PAR;
  uint32_t M0AR;
  uint32_t M1AR;
  uint32_t NDTR;
  uint32_t CR;
  uint32_t FCR;
} DMA_Stream_TypeDef;

typedef struct {
  uint32_t LIFCR;
} DMA_TypeDef;

typedef struct {
  uint32_t CPAR;
  uint32_t CM0AR;
  uint32_t CM1AR;
  uint32_t CNDTR;
  uint32_t CCR;
} BDMA_Channel_TypeDef;

typedef struct {
  uint32_t IFCR;
} BDMA_TypeDef;

typedef struct {
  uint32_t CR1;
  uint32_t CR2;
  uint32_t FRCR;
  uint32_t SLOTR;
  uint32_t DR;
} SAI_Block_TypeDef;

typedef struct {
  uint32_t GCR;
} SAI_TypeDef;

typedef struct {
  uint32_t CHCFGR1;
} DFSDM_Channel_TypeDef;

typedef struct {
  uint32_t FLTFCR;
  uint32_t FLTCR1;
  uint32_t FLTRDATAR;
} DFSDM_Filter_TypeDef;

typedef struct {
  uint32_t DHR12R1;
  uint32_t DHR12R2;
  uint32_t DHR8R1;
  uint32_t DHR8R2;
  uint32_t MCR;
  uint32_t CR;
} DAC_TypeDef;

typedef struct {
  uint32_t CR3;
} PWR_TypeDef;

typedef struct {
  uint32_t PMCR;
  uint32_t EXTICR[4];
} SYSCFG_TypeDef;

typedef int IRQn_Type;

// Include driver declarations AFTER types are defined
#include "board/drivers/driver_declarations.h"

#define UNUSED(x) (void)(x)
#define ALLOW_DEBUG

#define ENTER_CRITICAL() 0
#define EXIT_CRITICAL() 0

void print(const char *a);
void puth(unsigned int i);

extern TIM_TypeDef *MICROSECOND_TIMER;
extern TIM_TypeDef *TICK_TIMER;
extern TIM_TypeDef *INTERRUPT_TIMER;
extern TIM_TypeDef *TIM1;
extern TIM_TypeDef *TIM3;
extern TIM_TypeDef *TIM5;
extern TIM_TypeDef *TIM7;
extern TIM_TypeDef *TIM8;
extern TIM_TypeDef *TIM12;

uint32_t microsecond_timer_get(void);

extern GPIO_TypeDef *GPIOA;
extern GPIO_TypeDef *GPIOB;
extern GPIO_TypeDef *GPIOC;
extern GPIO_TypeDef *GPIOD;
extern GPIO_TypeDef *GPIOE;
extern GPIO_TypeDef *GPIOF;
extern GPIO_TypeDef *GPIOG;
extern GPIO_TypeDef *GPIOH;

extern USART_TypeDef *USART1;
extern USART_TypeDef *USART2;
extern USART_TypeDef *USART3;
extern USART_TypeDef *UART7;

extern FDCAN_GlobalTypeDef *FDCAN1;
extern FDCAN_GlobalTypeDef *FDCAN2;
extern FDCAN_GlobalTypeDef *FDCAN3;

extern ADC_TypeDef *ADC1;
extern ADC_TypeDef *ADC2;
extern ADC_TypeDef *ADC3;

extern DMAMUX_Channel_TypeDef *DMAMUX1_Channel1;
extern DMAMUX_Channel_TypeDef *DMAMUX2_Channel0;
extern DMAMUX_Channel_TypeDef *DMAMUX2_Channel1;
extern DMAMUX_Channel_TypeDef *DMAMUX1_Channel0;

extern DMA_Stream_TypeDef *DMA1_Stream0;
extern DMA_Stream_TypeDef *DMA1_Stream1;

extern DMA_TypeDef *DMA1;

extern BDMA_Channel_TypeDef *BDMA_Channel0;
extern BDMA_Channel_TypeDef *BDMA_Channel1;

extern BDMA_TypeDef *BDMA;

extern SAI_Block_TypeDef *SAI4_Block_A;
extern SAI_Block_TypeDef *SAI4_Block_B;

extern SAI_TypeDef *SAI4;

extern DFSDM_Channel_TypeDef *DFSDM1_Channel0;
extern DFSDM_Channel_TypeDef *DFSDM1_Channel3;

extern DFSDM_Filter_TypeDef *DFSDM1_Filter0;

extern DAC_TypeDef *DAC1;

extern PWR_TypeDef *PWR;

extern SYSCFG_TypeDef *SYSCFG;

#define PWR_CR3_USBREGEN 0
#define PWR_CR3_USB33DEN 0
#define PWR_CR3_USB33RDY 1

#endif
