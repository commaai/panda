// Fake STM32 hardware simulation for GPIO initialization testing.
// Provides simulated GPIO registers so board init functions can be called
// and the resulting GPIO configuration can be verified.
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#define ALLOW_DEBUG

// Critical section stubs
#define ENTER_CRITICAL() 0
#define EXIT_CRITICAL() 0

// Print stubs
void print(const char *a) {
  printf("%s", a);
}

void puth(unsigned int i) {
  printf("%u", i);
}

// Interrupt stubs
#define __disable_irq()
#define __enable_irq()
#define __IO volatile

// ==================== GPIO Simulation ====================

typedef struct {
  volatile uint32_t MODER;
  volatile uint32_t OTYPER;
  volatile uint32_t OSPEEDR;
  volatile uint32_t PUPDR;
  volatile uint32_t IDR;
  volatile uint32_t ODR;
  volatile uint32_t BSRR;
  volatile uint32_t LCKR;
  volatile uint32_t AFR[2];
} GPIO_TypeDef;

// Simulated GPIO banks
static GPIO_TypeDef _GPIOA, _GPIOB, _GPIOC, _GPIOD, _GPIOE, _GPIOF, _GPIOG, _GPIOH;

#define GPIOA (&_GPIOA)
#define GPIOB (&_GPIOB)
#define GPIOC (&_GPIOC)
#define GPIOD (&_GPIOD)
#define GPIOE (&_GPIOE)
#define GPIOF (&_GPIOF)
#define GPIOG (&_GPIOG)
#define GPIOH (&_GPIOH)

// GPIO OSPEEDR bit definitions
#define GPIO_OSPEEDR_OSPEED11  (3UL << 22)
#define GPIO_OSPEEDR_OSPEED12  (3UL << 24)
#define GPIO_OSPEEDR_OSPEED13  (3UL << 26)
#define GPIO_OSPEEDR_OSPEED14  (3UL << 28)

// GPIO OTYPER bit definitions
#define GPIO_OTYPER_OT8   (1UL << 8)
#define GPIO_OTYPER_OT10  (1UL << 10)
#define GPIO_OTYPER_OT11  (1UL << 11)

// GPIO Alternate Function definitions
#define GPIO_AF1_TIM1       ((uint8_t)0x01)
#define GPIO_AF2_TIM3       ((uint8_t)0x02)
#define GPIO_AF2_FDCAN3     ((uint8_t)0x02)
#define GPIO_AF3_DFSDM1     ((uint8_t)0x03)
#define GPIO_AF4_I2C5       ((uint8_t)0x04)
#define GPIO_AF5_SPI4       ((uint8_t)0x05)
#define GPIO_AF5_FDCAN3     ((uint8_t)0x05)
#define GPIO_AF7_USART2     ((uint8_t)0x07)
#define GPIO_AF7_UART7      ((uint8_t)0x07)
#define GPIO_AF8_SAI4       ((uint8_t)0x08)
#define GPIO_AF9_FDCAN1     ((uint8_t)0x09)
#define GPIO_AF9_FDCAN2     ((uint8_t)0x09)
#define GPIO_AF10_SAI4      ((uint8_t)0x0A)
#define GPIO_AF10_OTG1_FS   ((uint8_t)0x0A)

// ==================== Timer Simulation ====================

typedef struct {
  volatile uint32_t CR1;
  volatile uint32_t CR2;
  volatile uint32_t SMCR;
  volatile uint32_t DIER;
  volatile uint32_t SR;
  volatile uint32_t EGR;
  volatile uint32_t CCMR1;
  volatile uint32_t CCMR2;
  volatile uint32_t CCER;
  volatile uint32_t CNT;
  volatile uint32_t PSC;
  volatile uint32_t ARR;
  volatile uint32_t RCR;
  volatile uint32_t CCR1;
  volatile uint32_t CCR2;
  volatile uint32_t CCR3;
  volatile uint32_t CCR4;
  volatile uint32_t BDTR;
  volatile uint32_t DCR;
  volatile uint32_t DMAR;
  uint32_t RESERVED1;
  volatile uint32_t CCMR3;
  volatile uint32_t CCR5;
  volatile uint32_t CCR6;
  volatile uint32_t AF1;
  volatile uint32_t AF2;
  volatile uint32_t TISEL;
} TIM_TypeDef;

static TIM_TypeDef _TIM1, _TIM2, _TIM3, _TIM5, _TIM6, _TIM7, _TIM8, _TIM12;
#define TIM1 (&_TIM1)
#define TIM2 (&_TIM2)
#define TIM3 (&_TIM3)
#define TIM5 (&_TIM5)
#define TIM6 (&_TIM6)
#define TIM7 (&_TIM7)
#define TIM8 (&_TIM8)
#define TIM12 (&_TIM12)

#define MICROSECOND_TIMER TIM2

// Timer register bit definitions
#define TIM_CR1_CEN    (1UL << 0)
#define TIM_CR1_ARPE   (1UL << 7)
#define TIM_CR1_URS    (1UL << 2)
#define TIM_CR2_MMS_Pos 4
#define TIM_CR2_MMS_Msk (7UL << 4)
#define TIM_EGR_UG     (1UL << 0)
#define TIM_DIER_UIE   (1UL << 0)
#define TIM_DIER_CC1IE (1UL << 1)
#define TIM_BDTR_MOE   (1UL << 15)

#define TIM_CCMR1_OC1M_Pos 4
#define TIM_CCMR1_OC1M_1 (2UL << 4)
#define TIM_CCMR1_OC1M_2 (4UL << 4)
#define TIM_CCMR1_OC1PE  (1UL << 3)
#define TIM_CCMR1_OC2M_Pos 12
#define TIM_CCMR1_OC2M_1 (2UL << 12)
#define TIM_CCMR1_OC2M_2 (4UL << 12)
#define TIM_CCMR1_OC2PE  (1UL << 11)

#define TIM_CCMR2_OC3M_Pos 4
#define TIM_CCMR2_OC3M_1 (2UL << 4)
#define TIM_CCMR2_OC3M_2 (4UL << 4)
#define TIM_CCMR2_OC3PE  (1UL << 3)
#define TIM_CCMR2_OC4M_Pos 12
#define TIM_CCMR2_OC4M_1 (2UL << 12)
#define TIM_CCMR2_OC4M_2 (4UL << 12)
#define TIM_CCMR2_OC4PE  (1UL << 11)

#define TIM_CCER_CC1E  (1UL << 0)
#define TIM_CCER_CC2E  (1UL << 4)
#define TIM_CCER_CC2NE (1UL << 6)
#define TIM_CCER_CC3NE (1UL << 10)

// ==================== ADC Simulation ====================

typedef struct {
  volatile uint32_t dummy;
} ADC_TypeDef;

static ADC_TypeDef _ADC1, _ADC3;
#define ADC1 (&_ADC1)
#define ADC3 (&_ADC3)

typedef enum {
  SAMPLETIME_1_CYCLE = 0,
  SAMPLETIME_2_CYCLES = 1,
  SAMPLETIME_8_CYCLES = 2,
  SAMPLETIME_16_CYCLES = 3,
  SAMPLETIME_32_CYCLES = 4,
  SAMPLETIME_64_CYCLES = 5,
  SAMPLETIME_387_CYCLES = 6,
  SAMPLETIME_810_CYCLES = 7
} adc_sample_time_t;

typedef enum {
  OVERSAMPLING_1 = 0,
  OVERSAMPLING_64 = 6
} adc_oversampling_t;

typedef struct {
  ADC_TypeDef *adc;
  uint8_t channel;
  adc_sample_time_t sample_time;
  adc_oversampling_t oversampling;
} adc_signal_t;

#define ADC_CHANNEL_DEFAULT(a, c) {.adc = (a), .channel = (c), .sample_time = SAMPLETIME_32_CYCLES, .oversampling = OVERSAMPLING_64}

uint32_t adc_get_mV(const adc_signal_t *signal) {
  (void)signal;
  return 0;
}

// ==================== PWR Simulation ====================

typedef struct {
  volatile uint32_t CR1;
  volatile uint32_t CSR1;
  volatile uint32_t CR2;
  volatile uint32_t CR3;
} PWR_TypeDef;

static PWR_TypeDef _PWR;
#define PWR (&_PWR)

#define PWR_CR3_USBREGEN  (1UL << 25)
#define PWR_CR3_USB33DEN  (1UL << 24)
#define PWR_CR3_USB33RDY  (1UL << 26)

// ==================== RCC Simulation ====================

typedef struct {
  volatile uint32_t CR;
  volatile uint32_t AHB1ENR;
  volatile uint32_t AHB2ENR;
  volatile uint32_t AHB3ENR;
  volatile uint32_t AHB4ENR;
  volatile uint32_t APB1LENR;
  volatile uint32_t APB1HENR;
  volatile uint32_t APB2ENR;
  volatile uint32_t APB4ENR;
  volatile uint32_t AHB1LPENR;
} RCC_TypeDef;

static RCC_TypeDef _RCC;
#define RCC (&_RCC)

#define RCC_AHB4ENR_GPIOAEN  (1UL << 0)
#define RCC_AHB4ENR_GPIOBEN  (1UL << 1)
#define RCC_AHB4ENR_GPIOCEN  (1UL << 2)
#define RCC_AHB4ENR_GPIODEN  (1UL << 3)
#define RCC_AHB4ENR_GPIOEEN  (1UL << 4)
#define RCC_AHB4ENR_GPIOFEN  (1UL << 5)
#define RCC_AHB4ENR_GPIOGEN  (1UL << 6)
#define RCC_AHB4ENR_GPIOHEN  (1UL << 7)
#define RCC_AHB4ENR_ADC3EN   (1UL << 24)
#define RCC_AHB4ENR_BDMAEN   (1UL << 21)
#define RCC_AHB1ENR_ADC12EN  (1UL << 5)
#define RCC_AHB1ENR_DMA1EN   (1UL << 0)
#define RCC_AHB1ENR_DMA2EN   (1UL << 1)
#define RCC_AHB1ENR_USB1OTGHSEN (1UL << 25)
#define RCC_AHB1LPENR_USB1OTGHSLPEN (1UL << 25)
#define RCC_AHB1LPENR_USB1OTGHSULPILPEN (1UL << 26)
#define RCC_AHB2ENR_SRAM1EN  (1UL << 29)
#define RCC_AHB2ENR_SRAM2EN  (1UL << 30)
#define RCC_APB1LENR_I2C5EN  (1UL << 25)
#define RCC_APB1LENR_TIM2EN  (1UL << 0)
#define RCC_APB1LENR_TIM3EN  (1UL << 1)
#define RCC_APB1LENR_TIM5EN  (1UL << 3)
#define RCC_APB1LENR_TIM6EN  (1UL << 4)
#define RCC_APB1LENR_TIM7EN  (1UL << 5)
#define RCC_APB1LENR_TIM12EN (1UL << 6)
#define RCC_APB1LENR_UART7EN (1UL << 30)
#define RCC_APB1LENR_DAC12EN (1UL << 29)
#define RCC_APB1HENR_FDCANEN (1UL << 8)
#define RCC_APB2ENR_SPI4EN   (1UL << 13)
#define RCC_APB2ENR_TIM1EN   (1UL << 0)
#define RCC_APB2ENR_TIM8EN   (1UL << 1)
#define RCC_APB2ENR_DFSDM1EN (1UL << 30)
#define RCC_APB4ENR_SYSCFGEN (1UL << 1)
#define RCC_APB4ENR_SAI4EN   (1UL << 21)

// ==================== NVIC Simulation ====================

typedef int IRQn_Type;
#define TIM1_UP_TIM10_IRQn 25
#define TIM1_CC_IRQn 27

static inline void NVIC_DisableIRQ(IRQn_Type irq) { (void)irq; }

// ==================== DMA / DAC / I2C / BDMA / DMAMUX Stubs ====================

typedef struct { volatile uint32_t CR; volatile uint32_t NDTR; volatile uint32_t PAR; volatile uint32_t M0AR; volatile uint32_t FCR; } DMA_Stream_TypeDef;
typedef struct { volatile uint32_t CCR; } DMAMUX_Channel_TypeDef;
typedef struct { volatile uint32_t CCR; } BDMA_Channel_TypeDef;
typedef struct { volatile uint32_t DHR8R1; volatile uint32_t DHR8R2; volatile uint32_t MCR; volatile uint32_t CR; } DAC_TypeDef;
typedef struct { volatile uint32_t dummy; } I2C_TypeDef;
typedef struct { volatile uint32_t CR1; volatile uint32_t CR2; volatile uint32_t CR3; volatile uint32_t BRR; } USART_TypeDef;
typedef struct { volatile uint32_t dummy; } FDCAN_GlobalTypeDef;

static DMA_Stream_TypeDef _DMA1_Stream1;
static DMAMUX_Channel_TypeDef _DMAMUX1_Channel1;
static BDMA_Channel_TypeDef _BDMA_Channel0;
static DAC_TypeDef _DAC1;
static I2C_TypeDef _I2C5;
static USART_TypeDef _UART7;

#define DMA1_Stream1 (&_DMA1_Stream1)
#define DMAMUX1_Channel1 (&_DMAMUX1_Channel1)
#define BDMA_Channel0 (&_BDMA_Channel0)
#define DAC1 (&_DAC1)
#define I2C5 (&_I2C5)
#define UART7 (&_UART7)

#define DMA_SxCR_PL_Pos   16
#define DMA_SxCR_MINC     (1UL << 10)
#define DMA_SxCR_CIRC     (1UL << 8)
#define DMA_SxCR_DIR_Pos  6
#define DMA_SxCR_EN       (1UL << 0)
#define DMAMUX_CxCR_DMAREQ_ID_Msk 0xFFU
#define BDMA_CCR_EN       (1UL << 0)
#define DAC_CR_TEN1       (1UL << 1)
#define DAC_CR_TSEL1_Pos  2
#define DAC_CR_DMAEN1     (1UL << 12)
#define DAC_CR_EN1        (1UL << 0)
#define DAC_CR_EN2        (1UL << 16)

// ==================== Other constants ====================

#define APB1_TIMER_FREQ 60U
#define APB2_TIMER_FREQ 60U

#define PANDA_CAN_CNT 3U
#define CANPACKET_DATA_SIZE_MAX 64U

#define REGISTER_MAP_SIZE 0x3FFU
#define HASHING_PRIME 23U

#define NUM_INTERRUPTS 163U

// ==================== Stub uart ring ====================
typedef struct uart_ring {
  volatile uint16_t w_ptr_tx;
  volatile uint16_t r_ptr_tx;
  uint8_t *elems_tx;
  uint32_t tx_fifo_size;
  volatile uint16_t w_ptr_rx;
  volatile uint16_t r_ptr_rx;
  uint8_t *elems_rx;
  uint32_t rx_fifo_size;
  USART_TypeDef *uart;
  void (*callback)(struct uart_ring*);
  bool overwrite;
} uart_ring;

static uart_ring uart_ring_som_debug;
void uart_init(uart_ring *ring, uint32_t baud) { (void)ring; (void)baud; }

// ==================== Stub I2C / sound functions ====================
void i2c_init(I2C_TypeDef *i2c) { (void)i2c; }
bool i2c_set_reg_bits(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint8_t bits) { (void)i2c; (void)addr; (void)reg; (void)bits; return true; }
bool i2c_clear_reg_bits(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint8_t bits) { (void)i2c; (void)addr; (void)reg; (void)bits; return true; }
bool i2c_set_reg_mask(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint8_t val, uint8_t mask) { (void)i2c; (void)addr; (void)reg; (void)val; (void)mask; return true; }
void sound_init(void) {}
void sound_stop_dac(void) {}
void sound_init_dac(void) {}

// ==================== Other stubs ====================
uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}

// Reset all simulated hardware registers
void reset_fake_gpio(void) {
  memset(&_GPIOA, 0, sizeof(GPIO_TypeDef));
  memset(&_GPIOB, 0, sizeof(GPIO_TypeDef));
  memset(&_GPIOC, 0, sizeof(GPIO_TypeDef));
  memset(&_GPIOD, 0, sizeof(GPIO_TypeDef));
  memset(&_GPIOE, 0, sizeof(GPIO_TypeDef));
  memset(&_GPIOF, 0, sizeof(GPIO_TypeDef));
  memset(&_GPIOG, 0, sizeof(GPIO_TypeDef));
  memset(&_GPIOH, 0, sizeof(GPIO_TypeDef));
  memset(&_TIM1, 0, sizeof(TIM_TypeDef));
  memset(&_TIM3, 0, sizeof(TIM_TypeDef));
  memset(&_PWR, 0, sizeof(PWR_TypeDef));
  memset(&_RCC, 0, sizeof(RCC_TypeDef));
  // PWR: Set USB33RDY so tres_init doesn't loop forever
  _PWR.CR3 = PWR_CR3_USB33RDY;
}

// Additional TIM CCER bits needed by pwm.h
#define TIM_CCER_CC3E  (1UL << 8)
#define TIM_CCER_CC4E  (1UL << 12)
