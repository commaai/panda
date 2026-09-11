#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ALLOW_DEBUG
#define ENTER_CRITICAL() ((void)0)
#define EXIT_CRITICAL() ((void)0)

typedef struct {
  uint32_t CNT;
} TIM_TypeDef;

typedef uint32_t GPIO_TypeDef;
typedef uint32_t USART_TypeDef;
typedef int IRQn_Type;

extern TIM_TypeDef timer;
extern TIM_TypeDef *MICROSECOND_TIMER;
uint32_t microsecond_timer_get(void);
void print(const char *a);
void puth(unsigned int i);
