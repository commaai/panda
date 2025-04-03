// minimal code to fake a panda for tests
#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define CANFD
#define ALLOW_DEBUG
#define PANDA
#define FAULT_RELAY_MALFUNCTION             (1UL << 0)

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

TIM_TypeDef timer;
TIM_TypeDef *MICROSECOND_TIMER = &timer;


static inline uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}

typedef uint32_t GPIO_TypeDef;
