#include "fake_stm.h"

void print(const char *a) { printf("%s", a); }
void puth(unsigned int i) { printf("%u", i); }

TIM_TypeDef _timer;
TIM_TypeDef *MICROSECOND_TIMER = &_timer;
TIM_TypeDef *TICK_TIMER = &_timer;
TIM_TypeDef *INTERRUPT_TIMER = &_timer;
TIM_TypeDef *TIM1 = &_timer;
TIM_TypeDef *TIM3 = &_timer;
TIM_TypeDef *TIM5 = &_timer;
TIM_TypeDef *TIM7 = &_timer;
TIM_TypeDef *TIM8 = &_timer;
TIM_TypeDef *TIM12 = &_timer;

uint32_t microsecond_timer_get(void) { return MICROSECOND_TIMER->CNT; }

GPIO_TypeDef _gpio;
GPIO_TypeDef *GPIOA = &_gpio;
GPIO_TypeDef *GPIOB = &_gpio;
GPIO_TypeDef *GPIOC = &_gpio;
GPIO_TypeDef *GPIOD = &_gpio;
GPIO_TypeDef *GPIOE = &_gpio;
GPIO_TypeDef *GPIOF = &_gpio;
GPIO_TypeDef *GPIOG = &_gpio;
GPIO_TypeDef *GPIOH = &_gpio;

USART_TypeDef _usart;
USART_TypeDef *USART1 = &_usart;
USART_TypeDef *USART2 = &_usart;
USART_TypeDef *USART3 = &_usart;
USART_TypeDef *UART7 = &_usart;

FDCAN_GlobalTypeDef _fdcan;
FDCAN_GlobalTypeDef *FDCAN1 = &_fdcan;
FDCAN_GlobalTypeDef *FDCAN2 = &_fdcan;
FDCAN_GlobalTypeDef *FDCAN3 = &_fdcan;

ADC_TypeDef _adc;
ADC_TypeDef *ADC1 = &_adc;
ADC_TypeDef *ADC2 = &_adc;
ADC_TypeDef *ADC3 = &_adc;

DMAMUX_Channel_TypeDef _dmamux_chan;
DMAMUX_Channel_TypeDef *DMAMUX1_Channel1 = &_dmamux_chan;
DMAMUX_Channel_TypeDef *DMAMUX2_Channel0 = &_dmamux_chan;
DMAMUX_Channel_TypeDef *DMAMUX2_Channel1 = &_dmamux_chan;
DMAMUX_Channel_TypeDef *DMAMUX1_Channel0 = &_dmamux_chan;

DMA_Stream_TypeDef _dma_stream;
DMA_Stream_TypeDef *DMA1_Stream0 = &_dma_stream;
DMA_Stream_TypeDef *DMA1_Stream1 = &_dma_stream;

DMA_TypeDef _dma;
DMA_TypeDef *DMA1 = &_dma;

BDMA_Channel_TypeDef _bdma_chan;
BDMA_Channel_TypeDef *BDMA_Channel0 = &_bdma_chan;
BDMA_Channel_TypeDef *BDMA_Channel1 = &_bdma_chan;

BDMA_TypeDef _bdma;
BDMA_TypeDef *BDMA = &_bdma;

SAI_Block_TypeDef _sai_block;
SAI_Block_TypeDef *SAI4_Block_A = &_sai_block;
SAI_Block_TypeDef *SAI4_Block_B = &_sai_block;

SAI_TypeDef _sai;
SAI_TypeDef *SAI4 = &_sai;

DFSDM_Channel_TypeDef _dfsdm_chan;
DFSDM_Channel_TypeDef *DFSDM1_Channel0 = &_dfsdm_chan;
DFSDM_Channel_TypeDef *DFSDM1_Channel3 = &_dfsdm_chan;

DFSDM_Filter_TypeDef _dfsdm_filter;
DFSDM_Filter_TypeDef *DFSDM1_Filter0 = &_dfsdm_filter;

DAC_TypeDef _dac;
DAC_TypeDef *DAC1 = &_dac;

PWR_TypeDef _pwr;
PWR_TypeDef *PWR = &_pwr;

SYSCFG_TypeDef _syscfg;
SYSCFG_TypeDef *SYSCFG = &_syscfg;
