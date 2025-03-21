#pragma once
// minimal code to fake a panda for tests
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils.h"

#define CANFD
#ifndef ALLOW_DEBUG
#define ALLOW_DEBUG
#endif
#ifndef PANDA
#define PANDA
#endif

#define ENTER_CRITICAL() 0
#define EXIT_CRITICAL() 0

void print(const char *a);

void puth(unsigned int i);

typedef struct {
  uint32_t CNT;
} TIM_TypeDef;

TIM_TypeDef timer;
TIM_TypeDef *MICROSECOND_TIMER;
uint32_t microsecond_timer_get(void);

uint32_t microsecond_timer_get(void);
