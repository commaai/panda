#pragma once

#ifdef STM32H7
#error This code only makes sense on a non stm32h7 platform
#endif

// minimal code to fake a panda for tests
#include <stdint.h>

#include "utils.h"

#define ALLOW_DEBUG

#define ENTER_CRITICAL() 0
#define EXIT_CRITICAL() 0

void print(const char *a);
void puth(unsigned int i);

typedef struct {
  uint32_t CNT;
} TIM_TypeDef;

extern TIM_TypeDef timer;
extern TIM_TypeDef *MICROSECOND_TIMER;

uint32_t microsecond_timer_get(void);

typedef uint32_t GPIO_TypeDef;
typedef int IRQn_Type;
typedef uint32_t ADC_TypeDef;
typedef uint32_t USART_TypeDef;
typedef uint32_t FDCAN_GlobalTypeDef;
typedef uint32_t SPI_TypeDef;
typedef uint32_t USB_OTG_GlobalTypeDef;
typedef uint32_t USB_OTG_DeviceTypeDef;

#define NUM_INTERRUPTS 1
