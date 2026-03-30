// minimal code to fake a panda for tests
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils.h"

#define ALLOW_DEBUG

#define ENTER_CRITICAL() 0
#define EXIT_CRITICAL() 0

static inline void print(const char *a) {
  printf("%s", a);
}

static inline void puth(unsigned int i) {
  printf("%u", i);
}

typedef struct {
  uint32_t CNT;
} TIM_TypeDef;

static TIM_TypeDef timer;
static TIM_TypeDef *MICROSECOND_TIMER = &timer;

static inline uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}

typedef uint32_t GPIO_TypeDef;
