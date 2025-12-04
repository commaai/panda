#pragma once
// minimal code to fake a panda for tests
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils.h"

#define ALLOW_DEBUG

#ifndef ENTER_CRITICAL
#define ENTER_CRITICAL() 0
#endif
#ifndef EXIT_CRITICAL
#define EXIT_CRITICAL() 0
#endif

void print(const char *a);
void puth(unsigned int i);

typedef struct {
  uint32_t CNT;
} TIM_TypeDef;

extern TIM_TypeDef timer;
extern TIM_TypeDef *MICROSECOND_TIMER;

uint32_t microsecond_timer_get(void);

typedef uint32_t GPIO_TypeDef;
